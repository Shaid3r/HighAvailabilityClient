#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ftw.h>
#include <cerrno>
#include <cstring>
#include <sstream>


bool doesFileExists(const std::string& filepath) {
    struct stat buffer;   
    return (stat(filepath.c_str(), &buffer) == 0); 
}

off_t getFileSize(const std::string& filepath) {
    struct stat buffer;   
    if (stat(filepath.c_str(), &buffer) != 0){
        perror("stat: ");
        exit(EXIT_FAILURE);
    }
    return buffer.st_size;
}

unsigned int getNumberOfChunks(off_t dataSize) {
    if (dataSize % CHUNK_SIZE == 0) {
        return dataSize / CHUNK_SIZE;
    } else {
        return dataSize / CHUNK_SIZE + 1;
    }
}

std::string ip_to_str(const struct sockaddr *sa) {
    std::ostringstream ss;
    uint32_t port;
    char ip[INET6_ADDRSTRLEN];
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), ip, sizeof(ip));
            port = htons(((struct sockaddr_in *)sa)->sin_port);
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), ip, sizeof(ip));
            port = htons(((struct sockaddr_in6 *)sa)->sin6_port);
            break;
        default:
            return "Invalid ip";
    }
    ss << ip << ":" << port;
    return ss.str();
}

int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int removeDir(const char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}
