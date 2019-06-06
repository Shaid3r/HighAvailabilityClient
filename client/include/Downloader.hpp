#pragma once

#include <iostream>
#include <memory>
#include <unordered_map>
#include <sys/epoll.h>
#include "DiskWriter.hpp"
#include "Worker.hpp"


class Downloader {
public:
    Downloader() {
        epFd = epoll_create1(0);
        if (epFd == -1) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
        }
        workers.reserve(10);
        metaDataProvider = std::make_unique<MetaDataProvider>();
        chunkScheduler = std::make_unique<ChunkScheduler>(*metaDataProvider);
        diskWriter = std::make_unique<DiskWriter>(*metaDataProvider, *chunkScheduler);
    }

    ~Downloader() {
        tryClose(epFd, "Failed to close epFd");
    }

    void addServer(const std::string& hostname, const std::string& port) {
        try {
            std::unique_ptr<Worker> worker = std::make_unique<Worker>(epFd, hostname, port,
                    *chunkScheduler, *metaDataProvider, *diskWriter);
            workers[worker->getServerSock()] = std::move(worker);
        } catch (const std::exception& e) {
            std::cerr << "Could not connect to: " << hostname << ":" << port << std::endl;
            std::cerr << e.what() << std::endl;
        }
    }

    const std::unordered_map<uint64_t, std::string>& downloadChunks() {
        if (workers.empty()) {
            throw std::runtime_error("Could not connect to any server");
        }
        try {
            epoll_event events[MAX_EVENTS];
            while(!workers.empty()) {
                int readyCount = epoll_wait(epFd, events, MAX_EVENTS, TIMEOUT);
                if (readyCount == -1) {
                    perror("epoll_wait");
                    throw std::exception();
                } else if (readyCount == 0){
                    throw std::runtime_error("Timeout");
                }

                for (int i = 0; i < readyCount; ++i) {
                    try {
                        workers[events[i].data.fd]->notify();
                    } catch (const ChunkScheduler::NoMoreChunks&) {
                        workers.erase(events[i].data.fd);
                    }
                }
            }
        } catch(const ChunkScheduler::AllChunksDownloaded&) {
            std::cout << "All chunks are downloaded" << std::endl;
            workers.clear();
        }
        return chunkScheduler->getSavedChunks();
    }

    std::string getFilename() const {
        return metaDataProvider->getFilename();
    }
private:
    const int MAX_EVENTS{10};
    const int TIMEOUT{2000};

    std::unique_ptr<MetaDataProvider> metaDataProvider;
    std::unique_ptr<ChunkScheduler> chunkScheduler;
    std::unique_ptr<DiskWriter> diskWriter;

    std::unordered_map<int, std::unique_ptr<Worker>> workers;
    int epFd;
};
