#pragma once

#include <sys/epoll.h>
#include <vector>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include <fcntl.h>
#include <netdb.h>
#include <bits/unique_ptr.h>
#include <cassert>
#include "utils.hpp"
#include "proto.hpp"
#include "ChunkScheduler.hpp"
#include "MetaDataProvider.hpp"
#include "ByteArray.hpp"
#include "DiskWriter.hpp"

class Worker {
public:
    Worker(int epfd, const std::string &hostname, const std::string &port,
           ChunkScheduler &chunkScheduler,
           MetaDataProvider &metaDataProvider,
           DiskWriter& diskWriter)
            : chunkScheduler(chunkScheduler),
              metaDataProvider(metaDataProvider),
              diskWriter(diskWriter) {
        addrinfo hints{}, *serverInfo = nullptr;
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        int rv;
        if ((rv = getaddrinfo(hostname.c_str(), port.c_str(), &hints,
                              &serverInfo)) != 0) {
            std::cerr << "getaddrinfo: " << gai_strerror(rv) << std::endl;
            freeaddrinfo(serverInfo);
            throw std::runtime_error(hostname + ":" + port);
        }

        addrinfo *rp;
        for (rp = serverInfo; rp != nullptr; rp = rp->ai_next) {
            if ((serverSock = socket(rp->ai_family, rp->ai_socktype,
                                     rp->ai_protocol)) == -1) {
                perror("socket");
                continue;
            }

            serverIp = ip_to_str(rp->ai_addr);
            if (connect(serverSock, rp->ai_addr, rp->ai_addrlen) != -1)
                break;  /* Success */

            if (close(serverSock) == -1) {
                perror("close");
                throw std::runtime_error(serverIp);
            }
        }

        freeaddrinfo(serverInfo);

        if (rp == nullptr) {
            throw std::runtime_error(std::string("Connection to server ") + serverIp + " failed");
        }

        if (fcntl(serverSock, F_SETFL, O_NONBLOCK) == -1) {
            perror("fcntl");
            close(serverSock);
            throw std::runtime_error(serverIp);
        }

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = serverSock;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverSock, &event) == -1) {
            perror("epoll_ctl");
            close(serverSock);
            throw std::runtime_error(serverIp);
        }
        std::cout << "Server " << serverIp << " successfully registered" << std::endl;
    }

    ~Worker() {
        disconnect();
    }

    void disconnect() {
        std::cout << "Disconnecting from " << serverIp << std::endl;
        if (serverSock != -1 && close(serverSock)) {
            std::string err = std::string("close(") + serverIp + ")";
            perror(err.c_str());
        }
    }

    void notify() {
        switch (state) {
            case STATE::INIT:
                readMetadata();
                break;
            case STATE::CHUNK_REQUEST:
                requestChunk();
                break;
            case STATE::DOWNLOADING:
                downloadChunk();
                break;
            case STATE::CLOSED:
            default:
                break;
        }
    }

    bool writeAll(void *msg, size_t count) {
        ssize_t rv = write(serverSock, (u_int8_t*)msg + sendBytes, count - sendBytes);
        if (rv == -1) {
            perror("write");
            throw std::runtime_error(serverIp);
        }
        sendBytes += rv;
//        std::cout << "Send bytes: " << rv << ", total: " << sendBytes
//                  << std::endl;
        if (sendBytes == count) {
            sendBytes = 0;
            return true;
        }
        return false;
    }

    bool readAll(size_t count) {
        assert(count <= BUF_SIZE);
        ssize_t rv = read(serverSock, buf, count - receivedBytes);
        if (rv == -1) {
            perror("read");
            throw std::runtime_error(serverIp);
        }
        receivedBytes += rv;
//        std::cout << "Received bytes: " << rv << ", total: " << receivedBytes << std::endl;
        if (receivedBytes == count) {
            receivedBytes = 0;
            return true;
        }
        return false;
    }

    void readMetadata() {
        int result = readAll(Proto::MSG_SIZE);
        if (!result)
            return;
        // TODO setMetaData
        char *start = reinterpret_cast<char *>(buf);
        start[255] = '\0';
        metaDataProvider.filename = std::string(start, strnlen(start, Proto::MAX_FILENAME_SIZE));
        std::string filename(start, strnlen(start, Proto::MAX_FILENAME_SIZE));
        start += Proto::MAX_FILENAME_SIZE;
        memcpy(&(metaDataProvider.filesize), start, sizeof(metaDataProvider.filesize));

        std::cout << "(" << serverIp << ") readMetadata - filename: " << metaDataProvider.filename << " filesize: "
                  << metaDataProvider.filesize << " bytes" << std::endl;

        state = STATE::CHUNK_REQUEST;
        requestChunk(true);
    }

    void requestChunk(bool newChunk = false) {
        if (newChunk) {
            try {
                chunkToDownload = chunkScheduler.getChunkToDownload();
                chunkSize = metaDataProvider.getSizeOfChunk(chunkToDownload);
            } catch (const ChunkScheduler::NoMoreChunks& e) {
                std::cout << "No more chunks to download, closing worker..." << std::endl;
                state = STATE::CLOSED;
                throw e;
            }
            writerFd = diskWriter.createFileFd(serverSock, chunkToDownload);
        }
        std::cout << "(" << serverIp << ")" << " requesting " << chunkToDownload << " chunk" << std::endl;
        int result = writeAll(&chunkToDownload, sizeof(chunkToDownload));
        if (!result)
            return;

        std::cout << "Requested chunk " << chunkToDownload << " from " << serverIp << std::endl;
        state = STATE::DOWNLOADING;
    }

    void downloadChunk() {
        u_int64_t bytesToRead{BUF_SIZE};
        if (chunkSize - receivedBytes < BUF_SIZE)
            bytesToRead = chunkSize - receivedBytes;

        ssize_t rv = read(serverSock, buf, bytesToRead);
        if (rv == -1) {
            perror("read");
            throw std::runtime_error(serverIp);
        }
        receivedBytes += rv;
        diskWriter.writeBuf(writerFd, buf, static_cast<size_t>(rv));

        if (receivedBytes == chunkSize) {
            diskWriter.saveChunk(writerFd);
            receivedBytes = 0;
            state = STATE::CHUNK_REQUEST;
            requestChunk(true);
        }
    }

    int getServerSock() const {
        return serverSock;
    }

private:
    enum STATE {
        INIT, CHUNK_REQUEST, DOWNLOADING, CLOSED
    };
    ChunkScheduler &chunkScheduler;
    MetaDataProvider &metaDataProvider;
    DiskWriter& diskWriter;

    u_int8_t buf[BUF_SIZE];
    size_t receivedBytes{0};
    size_t sendBytes{0};

    STATE state{INIT};
    std::string serverIp;
    u_int64_t chunkToDownload{static_cast<u_int64_t>(-1)};
    int serverSock{-1};
    int writerFd;
    u_int64_t chunkSize;
};
