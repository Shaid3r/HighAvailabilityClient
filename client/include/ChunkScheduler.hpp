#pragma once

#include <unordered_map>
#include "MetaDataProvider.hpp"

class ChunkScheduler {
public:
    ChunkScheduler(const MetaDataProvider &metaDataProvider)
            : metaDataProvider(metaDataProvider) {}

    struct AllChunksDownloaded : std::exception {};
    struct NoMoreChunks : std::exception {};

    void markChunkAsDone(u_int64_t chunkNo, const std::string& path) {
        if (savedChunks.find(chunkNo) == savedChunks.end())
            savedChunks[chunkNo] = path;

        if (savedChunks.size() == chunks)
            throw AllChunksDownloaded();
    }

    u_int64_t getChunkToDownload() {
        chunks = metaDataProvider.getNumberOfChunks();
        if (savedChunks.size() > chunks)
            throw NoMoreChunks();

        while (true) {
            uint64_t chunkToDownload = nextChunk++ % (chunks + 1);
            if (savedChunks.find(chunkToDownload) == savedChunks.end()) {
                return chunkToDownload;
            }
        }
    }

    const std::unordered_map<uint64_t, std::string>& getSavedChunks() const {
        return savedChunks;
    }

private:
    const MetaDataProvider &metaDataProvider;
    std::unordered_map<uint64_t, std::string> savedChunks;
    u_int64_t nextChunk{0};
    u_int64_t chunks{};
};
