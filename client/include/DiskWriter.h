#pragma once

#include <zconf.h>
#include <bits/unique_ptr.h>
#include <unordered_map>
#include <list>
#include <fcntl.h>
#include <sys/epoll.h>
#include <iostream>
#include "ByteArray.h"
#include "MetaDataProvider.h"
#include "ChunkScheduler.h"

class DiskWriter {
public:
    DiskWriter(int epfd, const MetaDataProvider &metaDataProvider, ChunkScheduler& chunkScheduler)
            : epfd(epfd), metaDataProvider(metaDataProvider), chunkScheduler(chunkScheduler) {
        removeDir("workspace");
        if (mkdir("workspace", S_IRWXU) != 0) {
            std::cerr << "Cannot create workspace directory" << std::endl;
            perror("mkdir");
            throw std::exception();
        }
        std::cout << "Workspace directory created!" << std::endl;
    }

    int createFileFd(int workerFd, u_int64_t chunkNo) {
        const std::string chunkFn = generateChunkFilename(workerFd, chunkNo);
        fd sockfd = open(chunkFn.c_str(), O_CREAT | O_WRONLY | O_EXCL, S_IRWXU);
        std::cout << "Creating " << chunkFn << std::endl;
        if (sockfd == -1) {
            perror("open");
            throw std::exception();
        }

        fdMap[sockfd] = FdInfo{};
        fdMap[sockfd].chunkFn = chunkFn;
        fdMap[sockfd].chunkNo = chunkNo;
        fdMap[sockfd].chunkSize = metaDataProvider.getSizeOfChunk(chunkNo);
        std::cout << "Fd to " << chunkFn << " successfully created" << std::endl;

        return sockfd;
    }

    bool writeBuf(int sockFd, std::unique_ptr<ByteArray> arr) {
        size_t bytesToSave = getBytesToSave(sockFd);

        std::cout << "bytesToSave: " << bytesToSave << std::endl;
        size_t saved{0};
        while(saved < bytesToSave) {
            ssize_t rv = write(sockFd, arr->value + saved, bytesToSave - saved);
            if (rv == -1) {
                perror("write");
                throw std::runtime_error(fdMap[sockFd].chunkFn);
            } else if (rv == 0) {
                std::cout << "No data" << std::endl;
            }

            saved += rv;
        }
        fdMap[sockFd].writtenBytes += saved;
        std::cout << fdMap[sockFd].chunkFn << " written bytes: " << fdMap[sockFd].writtenBytes << " chunkSize: " << fdMap[sockFd].chunkSize << std::endl;
        if (fdMap[sockFd].writtenBytes == fdMap[sockFd].chunkSize) {
            std::cout << "Chunk " << fdMap[sockFd].chunkFn << " saved" << std::endl;
            if (close(sockFd) == -1) {
                perror("close");
                throw std::runtime_error(fdMap[sockFd].chunkFn);
            }
//            chunkScheduler.notify(fdMap[sockFd].chunkNo, fdMap[sockFd].chunkFn);
            return true;
        }
        return false;
    }

private:
    std::string generateChunkFilename(int workerFd, u_int64_t chunkNo) const {
        return std::string("workspace/chunk") + std::to_string(chunkNo) + "_" +
               std::to_string(workerFd);
    }

    bool isLastBufOfChunk(int sockFd) const {
        return fdMap.at(sockFd).chunkSize - fdMap.at(sockFd).writtenBytes <= BUF_SIZE;
    }

    size_t getBytesToSave(int sockFd) const {
        if (fdMap.at(sockFd).chunkSize % BUF_SIZE && isLastBufOfChunk(sockFd))
            return fdMap.at(sockFd).chunkSize % BUF_SIZE;
        return BUF_SIZE;
    }

    using fd = int;

    struct FdInfo {
        std::string chunkFn;
        u_int64_t chunkNo{};
        u_int64_t chunkSize{0};
        u_int64_t writtenBytes{0};
    };

    static const int MAX_ARR_SIZE{20};
    const int epfd;
    const MetaDataProvider& metaDataProvider;
    ChunkScheduler& chunkScheduler;
    std::unordered_map<fd, FdInfo> fdMap;
};