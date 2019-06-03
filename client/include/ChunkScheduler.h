#pragma once

#include <cstddef>
#include <vector>
#include <unordered_set>
#include "MetaDataProvider.h"

class ChunkScheduler {
public:
    ChunkScheduler(int epfd, const MetaDataProvider &metaDataProvider)
            : epfd(epfd), metaDataProvider(metaDataProvider) {}


    void addSavedChunk() {

    }

    u_int64_t getChunkToDownload() {
        return 2;
    }

    const int epfd;
    const MetaDataProvider &metaDataProvider;
    u_int64_t chunks;
    std::unordered_set<int> savedChunks;
    std::unordered_set<int> scheduledChunks;
    size_t filesize{};
};