#pragma once
#include <cstdint>
#include <string>
#include <stdexcept>
#include <string.h>

constexpr int CHUNK_SIZE{4096 * 1024};
// constexpr int MSG_METADATA   = 1;
// constexpr int MSG_CHUNK_REQ  = 2;
// constexpr int MSG_CHUNK_RESP = 3;

struct MsgMetadata {
    static const int type = 1;
    std::string filename;
    u_int64_t filesize;
};

struct MsgChunkReq {
    static const int type = 2;
    u_int64_t nr_of_chunk;

    static void receive() {

    }

    static void send() {

    }
};

// struct MsgChunkResp {
//     static const int type = 3;
//     u_int64_t nr_of_chunk;
// };

class Proto {
public:
    Proto(int socket) {
        int rv = read(socket, msg, MSG_SIZE);
        if (rv != MSG_SIZE) {
            perror("read");
        }
        parse();
    }

    Proto(const std::string& filename, uint64_t filesize) 
      : filename(filename), filesize(filesize) {
        if (filename.size() >= MAX_FILENAME_SIZE) {
            std::string err("Filename is too long. Max: ");
            err += std::to_string(MAX_FILENAME_SIZE - 1);
            err += ", actual: " + std::to_string(filename.size()); 
            throw std::runtime_error(err);
        }
    }

    void parse() {
        char *start = (char *)msg;
        start[255] = '\0';
        filename = std::string(start, strnlen(start, MAX_FILENAME_SIZE));
        start += MAX_FILENAME_SIZE;
        memcpy(&filesize, start, sizeof(filesize));
    }

    void *raw_msg() const {
        char *start = (char *)msg;
        memcpy(start, filename.data(), MAX_FILENAME_SIZE);
        start[filename.size()] = '\0'; // required until C++11
        start += MAX_FILENAME_SIZE;
        memcpy(start, &filesize, sizeof(filesize));

        return (void *)msg;
    }

    unsigned int size() const {
        return sizeof(msg);
    }

    std::string filename;
    u_int64_t filesize;

    
    static const size_t MAX_FILENAME_SIZE{256};
    static const size_t FILESIZE{sizeof(u_int64_t)};
    static const size_t MSG_SIZE{MAX_FILENAME_SIZE + FILESIZE};
private:
    char msg[MSG_SIZE];
};
