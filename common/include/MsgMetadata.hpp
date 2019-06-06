#pragma once

#include <cstdint>
#include <string>
#include <stdexcept>

constexpr int CHUNK_SIZE{4096 * 1024};

class MsgMetadata {
public:
    MsgMetadata(const std::string &filename, u_int64_t filesize)
            : filename(filename), filesize(filesize) {
        if (filename.size() >= MAX_FILENAME_SIZE) {
            std::string err("Filename is too long. Max: ");
            err += std::to_string(MAX_FILENAME_SIZE - 1);
            err += ", actual: " + std::to_string(filename.size());
            throw std::runtime_error(err);
        }
    }

    MsgMetadata(const uint8_t *buf) {
        auto *start = reinterpret_cast<const char *>(buf);
        filename = std::string(start, strnlen(start, MAX_FILENAME_SIZE - 1));
        start += MAX_FILENAME_SIZE;
        memcpy(&(filesize), start, sizeof(filesize));
    }

    void *generateMsg() {
        auto *start = reinterpret_cast<char *>(msg);
        memcpy(start, filename.data(), MAX_FILENAME_SIZE - 1);
        start[filename.size()] = '\0'; // required until C++11
        start += MAX_FILENAME_SIZE;
        memcpy(start, &filesize, sizeof(filesize));

        return msg;
    }

    std::string getFilename() const {
        return filename;
    }

    u_int64_t getFilesize() const {
        return filesize;
    }

    static const size_t MAX_FILENAME_SIZE{256};
    static const size_t FILESIZE{sizeof(u_int64_t)};
    static const size_t MSG_SIZE{MAX_FILENAME_SIZE + FILESIZE};

private:
    char msg[MSG_SIZE];
    std::string filename;
    u_int64_t filesize{};
};
