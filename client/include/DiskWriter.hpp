#pragma once

#include <zconf.h>
#include <bits/unique_ptr.h>
#include <unordered_map>
#include <list>
#include <fcntl.h>
#include <sys/epoll.h>
#include <iostream>
#include <utils.hpp>
#include "MetaDataProvider.hpp"
#include "ChunkScheduler.hpp"

class DiskWriter {
public:
    DiskWriter(const MetaDataProvider &metaDataProvider, ChunkScheduler& chunkScheduler)
            : metaDataProvider(metaDataProvider), chunkScheduler(chunkScheduler) {
        removeRecursively("workspace");
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
        if (sockfd == -1) {
            perror("open");
            throw std::exception();
        }

        fdMap[sockfd] = FdInfo{};
        fdMap[sockfd].chunkFn = chunkFn;
        fdMap[sockfd].chunkNo = chunkNo;

        return sockfd;
    }

    void closeChunk(int sockFd) {
        std::cout << "Chunk " << fdMap[sockFd].chunkNo << " ("<< fdMap[sockFd].chunkFn << ") saved" << std::endl;
        tryClose(sockFd, "Failed to close" + fdMap[sockFd].chunkFn);
        chunkScheduler.markChunkAsDone(fdMap[sockFd].chunkNo, fdMap[sockFd].chunkFn);
    }

    void writeBuf(int sockFd, u_int8_t *arr, size_t bytesToSave) {
        try {
            tryWriteAll(sockFd, arr, bytesToSave);
        } catch (const std::exception&) {
            std::cerr << "Cannot write " << fdMap[sockFd].chunkFn << " to disk" << std::endl;
        }
    }

private:
    std::string generateChunkFilename(int workerFd, u_int64_t chunkNo) const {
        return std::string("workspace/chunk") + std::to_string(chunkNo) + "_" +
               std::to_string(workerFd);
    }

    using fd = int;

    struct FdInfo {
        std::string chunkFn;
        u_int64_t chunkNo{};
    };

    const MetaDataProvider& metaDataProvider;
    ChunkScheduler& chunkScheduler;
    std::unordered_map<fd, FdInfo> fdMap;
};