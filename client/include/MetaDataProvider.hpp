#pragma once

#include <string>

struct MetaDataProvider {
    void setMetaData(const std::string& filename, u_int64_t filesize) {
        if (filename.empty()) {
            this->filename = filename;
            this->filesize = filesize;
        }
    }

    u_int64_t getSizeOfChunk(u_int64_t chunkNo) const {
        if (chunkNo < getNumberOfChunks() - 1)
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
private:
};