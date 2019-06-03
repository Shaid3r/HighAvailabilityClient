#pragma once

#include <sys/epoll.h>
#include <vector>
#include "worker.hpp"
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include <fcntl.h>
#include <netdb.h>
#include "gzip.hpp"
#include "utils.hpp"
#include "proto.hpp"
#include <memory>


class DownloadOrchestrator {
public:
    DownloadOrchestrator(int maxEvents = 10) : MAX_EVENTS(maxEvents) {
        epfd = epoll_create1(0);
        if (epfd == -1) {
            perror("epoll_create1");
            exit(EXIT_FAILURE);
        }
        workers.reserve(10);
        metaDataProvider = std::make_unique<MetaDataProvider>();
        chunkScheduler = std::make_unique<ChunkScheduler>(epfd, *metaDataProvider);
    }

    ~DownloadOrchestrator() {
        std::cout << "Closing connection poll" << std::endl;
        if(close(epfd)) {
            perror("close(epfd)");
            exit(EXIT_FAILURE);
        }
    }

    void addServer(const std::string& hostname, const std::string& port) {
        try {
            std::unique_ptr<Worker> worker = std::make_unique<Worker>(epfd, hostname, port,
                    *chunkScheduler, *metaDataProvider);
            workers[worker->serverSock] = std::move(worker);
        } catch (const std::exception& e) {
            std::cerr << "Could not connect. Ignoring " << e.what() << std::endl;
        }
    }

    void start() {
        epoll_event events[MAX_EVENTS];
        while(true) { // TODO
            int readyCount = epoll_wait(epfd, events, MAX_EVENTS, -1);
            if (readyCount == -1) {
                perror("epoll_wait");
                exit(EXIT_FAILURE);
            }

            for (int i = 0; i < readyCount; ++i) {
                workers[events[i].data.fd]->notify();
            }
            return;
        }
    }

//    std::vector<Worker> workers;
    std::unique_ptr<ChunkScheduler> chunkScheduler;
    std::unique_ptr<MetaDataProvider> metaDataProvider;
    std::unordered_map<int, std::unique_ptr<Worker>> workers;
    int epfd;
    const int MAX_EVENTS;
    epoll_event event;
};
