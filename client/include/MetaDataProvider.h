#pragma once

#include <string>

struct MetaDataProvider {

    u_int64_t getSizeOfChunk(u_int64_t chunkNo) {
        return 1;
    }

    u_int64_t getNumberOfChunks() {
        return 1;
    }

    std::string filename;
    u_int64_t filesize{};
};