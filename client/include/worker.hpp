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
#include "utils.hpp"
#include "proto.hpp"
#include "ChunkScheduler.h"
#include "MetaDataProvider.h"
#include "ByteArray.h"

class Worker {
public:
    Worker(int epfd, const std::string &hostname, const std::string &port,
           ChunkScheduler &chunkScheduler,
           MetaDataProvider &metaDataProvider)
            : chunkScheduler(chunkScheduler),
              metaDataProvider(metaDataProvider) {
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
            std::cout << "Connecting to: " << serverIp;
            if (connect(serverSock, rp->ai_addr, rp->ai_addrlen) != -1) {
                std::cout << "\t\tconnected" << std::endl;
                break;  /* Success */
            }
            std::cout << std::endl;
            perror("connect");
            close(serverSock);
        }

        freeaddrinfo(serverInfo);

        if (rp == nullptr) {

            throw std::runtime_error(serverIp);
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
        std::cout << "Server " << serverIp << " successfully registered"
                  << std::endl;
        buf = std::make_unique<ByteArray>();
    }

    ~Worker() {
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
                readChunk();
                break;
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
        std::cout << "Send bytes: " << rv << ", total: " << sendBytes
                  << std::endl;
        if (sendBytes == count) {
            sendBytes = 0;
            return true;
        }
        return false;
    }

    bool readAll(size_t count) {
        assert(count < BUF_SIZE);
        ssize_t rv = read(serverSock, buf.get() + receivedBytes,
                          count - receivedBytes);
        if (rv == -1) {
            perror("read");
            throw std::runtime_error(serverIp);
        }
        receivedBytes += rv;
        std::cout << "Received bytes: " << rv << ", total: " << receivedBytes
                  << std::endl;
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
        // TODO
        char *start = reinterpret_cast<char *>(buf.get());
        start[255] = '\0';
        metaDataProvider.filename = std::string(start, strnlen(start, Proto::MAX_FILENAME_SIZE));
        std::string filename(start, strnlen(start, Proto::MAX_FILENAME_SIZE));
        start += Proto::MAX_FILENAME_SIZE;
        memcpy(&(metaDataProvider.filesize), start, sizeof(metaDataProvider.filesize));

        std::cout << "readMetadata - filename: " << metaDataProvider.filename << " filesize: "
                  << metaDataProvider.filesize << " bytes" << std::endl;

        state = STATE::CHUNK_REQUEST;
        requestChunk(true);
    }

    void requestChunk(bool newChunk = false) {
        if (newChunk) {
            chunkToDownload = chunkScheduler.getChunkToDownload();;
        }
        int result = writeAll(&chunkToDownload, sizeof(chunkToDownload));
        if (!result)
            return;

        std::cout << "Requested chunk " << chunkToDownload << " from " << serverIp << std::endl;
        state = STATE::DOWNLOADING;
        // downloadChunk ???
    }

    void readChunk() {
        std::cout << "ChunkRead" << std::endl;
        exit(0);
    }

    int serverSock{-1};
private:
    enum STATE {
        INIT, CHUNK_REQUEST, DOWNLOADING
    };
    ChunkScheduler &chunkScheduler;
    MetaDataProvider &metaDataProvider;
    std::unique_ptr<ByteArray> buf;
//    char buf[BUF_SIZE]{};
    size_t receivedBytes{0};
    size_t sendBytes{0};

    STATE state{INIT};
    std::string serverIp;
    u_int64_t chunkToDownload{static_cast<u_int64_t>(-1)};
};
