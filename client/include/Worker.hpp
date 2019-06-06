#pragma once

#include <iostream>
#include <cassert>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unordered_map>
#include "ChunkScheduler.hpp"
#include "DiskWriter.hpp"
#include "MetaDataProvider.hpp"
#include "MsgMetadata.hpp"
#include "utils.hpp"

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

            serverIp = ipToStr(rp->ai_addr);
            if (connect(serverSock, rp->ai_addr, rp->ai_addrlen) != -1)
                break;  /* Success */

            tryClose(serverSock, serverIp);
        }

        freeaddrinfo(serverInfo);

        if (rp == nullptr) {
            throw std::runtime_error(std::string("Connection to server ") + serverIp + " failed");
        }

        if (fcntl(serverSock, F_SETFL, O_NONBLOCK) == -1) {
            perror("fcntl");
            tryClose(serverSock, serverIp);
            throw std::runtime_error(serverIp);
        }

        epoll_event event{};
        event.events = EPOLLIN;
        event.data.fd = serverSock;

        if (epoll_ctl(epfd, EPOLL_CTL_ADD, serverSock, &event) == -1) {
            perror("epoll_ctl");
            tryClose(serverSock, serverIp);
            throw std::runtime_error(serverIp);
        }
        std::cout << "Server " << serverIp << " successfully registered" << std::endl;
    }

    ~Worker() {
        disconnect();
    }

    void notify() {
        switch (state) {
            case STATE::INIT:
                readMetadata();
                return;
            case STATE::CHUNK_REQUEST:
                requestChunk();
                return;
            case STATE::DOWNLOADING:
                downloadChunk();
                return;
            case STATE::CLOSED:
                return;
        }
    }

    int getServerSock() const {
        return serverSock;
    }

private:
    void disconnect() {
        std::cout << "Disconnecting from " << serverIp << std::endl;
        tryClose(serverSock, serverIp);
    }

    void readMetadata() {
        if (!readAllNoBlocking(MsgMetadata::MSG_SIZE))
            return;

        MsgMetadata msg(buf);
        metaDataProvider.setMetaData(msg.getFilename(), msg.getFilesize());
        std::cout << "(" << serverIp << ") readMetadata - filename: " << metaDataProvider.getFilename() << " filesize: "
                  << metaDataProvider.getFilesize() << " bytes" << std::endl;

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
        if (!writeAllNoBlocking(&chunkToDownload, sizeof(chunkToDownload)))
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
            diskWriter.closeChunk(writerFd);
            receivedBytes = 0;
            state = STATE::CHUNK_REQUEST;
            requestChunk(true);
        }
    }

    bool readAllNoBlocking(size_t count) {
        assert(count <= BUF_SIZE);
        ssize_t rv = read(serverSock, buf, count - receivedBytes);
        if (rv == -1) {
            perror("read");
            throw std::runtime_error(serverIp);
        }
        receivedBytes += rv;
        if (receivedBytes == count) {
            receivedBytes = 0;
            return true;
        }
        return false;
    }

    bool writeAllNoBlocking(void *msg, size_t count) {
        ssize_t rv = write(serverSock, (u_int8_t*)msg + sendBytes, count - sendBytes);
        if (rv == -1) {
            perror("write");
            throw std::runtime_error(serverIp);
        }
        sendBytes += rv;
        if (sendBytes == count) {
            sendBytes = 0;
            return true;
        }
        return false;
    }

    enum STATE {
        INIT, CHUNK_REQUEST, DOWNLOADING, CLOSED
    };
    static const u_int64_t BUF_SIZE{8192};

    ChunkScheduler &chunkScheduler;
    MetaDataProvider &metaDataProvider;
    DiskWriter& diskWriter;

    u_int8_t buf[BUF_SIZE];
    size_t receivedBytes{0};
    size_t sendBytes{0};

    STATE state{INIT};
    std::string serverIp;
    u_int64_t chunkSize;
    u_int64_t chunkToDownload;
    int serverSock{-1};
    int writerFd;
};
