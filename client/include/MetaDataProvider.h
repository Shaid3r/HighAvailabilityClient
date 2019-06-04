#pragma once

#include <string>

struct MetaDataProvider {

    u_int64_t getSizeOfChunk(u_int64_t chunkNo) const {
        if (chunkNo < getNumberOfChunks())
            return CHUNK_SIZE;
        return filesize % CHUNK_SIZE;
    }

    u_int64_t getNumberOfChunks() const {
        if (filesize % CHUNK_SIZE == 0) {
            return filesize / CHUNK_SIZE;
        } else {
            return filesize / CHUNK_SIZE + 1;
        }
    }

    std::string filename;
    u_int64_t filesize{};
};