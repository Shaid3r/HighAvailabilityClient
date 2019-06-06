#pragma once

#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <cerrno>
#include <cstring>
#include <sstream>


bool doesFileExists(const std::string &filepath) {
    struct stat buffer{};
    return (stat(filepath.c_str(), &buffer) == 0);
}

u_int64_t getFileSize(const std::string &filepath) {
    struct stat buffer{};
    if (stat(filepath.c_str(), &buffer) != 0) {
        perror("stat: ");
        exit(EXIT_FAILURE);
    }
    return static_cast<u_int64_t>(buffer.st_size);
}

u_int64_t getNumberOfChunks(u_int64_t dataSize, u_int64_t chunkSize) {
    if (dataSize % chunkSize == 0) {
        return dataSize / chunkSize;
    } else {
        return dataSize / chunkSize + 1;
    }
}

u_int64_t getSizeOfChunk(u_int64_t fileSize, u_int64_t chunkNo, u_int64_t chunkSize) {
    if (chunkNo < getNumberOfChunks(fileSize, chunkSize) - 1)
        return chunkSize;
    return fileSize % chunkSize;
}

std::string ipToStr(const struct sockaddr *sa) {
    std::ostringstream ss;
    uint32_t port;
    char ip[INET6_ADDRSTRLEN];
    switch (sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((sockaddr_in *) sa)->sin_addr), ip,
                      sizeof(ip));
            port = htons(((sockaddr_in *) sa)->sin_port);
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &(((sockaddr_in6 *) sa)->sin6_addr), ip,
                      sizeof(ip));
            port = htons(((sockaddr_in6 *) sa)->sin6_port);
            break;
        default:
            return "Invalid ip";
    }
    ss << ip << ":" << port;
    return ss.str();
}

int unlinkCb(const char *fpath, const struct stat *, int, struct FTW *) {
    int rv = remove(fpath);
    if (rv)
        perror(fpath);

    return rv;
}

int removeRecursively(const char *path) {
    return nftw(path, unlinkCb, 64, FTW_DEPTH | FTW_PHYS);
}

void tryWriteAll(int sockFd, void *msg, size_t count) {
    size_t written_bytes{0};

    while (written_bytes < count) {
        ssize_t rv = write(sockFd, (u_int8_t *) msg + written_bytes,
                           count - written_bytes);
        if (rv == -1) {
            perror("write");
            throw std::exception();
        }
        written_bytes += rv;
    }
}

void tryClose(int sockfd, const std::string& msg) {
    if (close(sockfd)) {
        perror("close");
        throw std::runtime_error(msg);
    }
}