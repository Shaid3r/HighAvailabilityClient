#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include "Gzip.hpp"
#include "MsgMetadata.hpp"
#include "utils.hpp"

class Server {
public:
    Server(int argc, char **argv) {
        std::cout << "Loading settings" << std::endl;
        load_settings(argc, argv);
        validate_settings();
        std::cout << "Preparing data" << std::endl;
        prepare_data();
        initSocket();
        handleConnections();
    }

    ~Server() {
        close(dataFd);
        close(serverSock);
    }

private:
    void prepare_data() {
        const std::string data_path = get_data_path();
        if (doesFileExists(data_path)) {
            std::cout << "Compressed data (" << data_path << ") already exists."
                      << std::endl;
        } else {
            std::cout << "Compressing data (" << data_path << ")." << std::endl;
            try {
                Gzip::compress(filepath, COMPRESSION_LEVEL, data_path);
            } catch (const std::runtime_error &e) {
                std::cout << e.what() << std::endl;
                exit(EXIT_FAILURE);
            }
        }
        dataSize = getFileSize(data_path);
        chunks = getNumberOfChunks(dataSize, CHUNK_SIZE);
        std::cout << "Compressed data has " << dataSize << " bytes (" << chunks
                  << " chunks)" << std::endl;
        dataFd = open(data_path.c_str(), O_RDONLY, 0);
    }

    void initSocket() {
        addrinfo hints{}, *serverInfo;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        int rv;
        if ((rv = getaddrinfo(nullptr, port.c_str(), &hints, &serverInfo)) != 0) {
            fprintf(stderr, "getaddinfo: %s\n", gai_strerror(rv));
            exit(1);
        }

        addrinfo *rp;
        for (rp = serverInfo; rp != nullptr; rp = rp->ai_next) {
            if ((serverSock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
                perror("socket");
                continue;
            }

            int enable = 1;
            if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                           &enable, sizeof(enable)) == -1) {
                perror("setsockopt");
                exit(EXIT_FAILURE);
            }

            if (bind(serverSock, rp->ai_addr, rp->ai_addrlen) == 0)
                break;
            perror("bind");
        }

        if (rp == nullptr) {
            fprintf(stderr, "Could not bind\n");
            exit(EXIT_FAILURE);
        }
        freeaddrinfo(serverInfo);
    }

    void handleConnections() {
        if (listen(serverSock, 5) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }
        std::cout << "Waiting for connections." << std::endl;

        int clientSock;
        sockaddr clientAddr{};
        socklen_t addrlen = sizeof(clientAddr);

        while (true) {
            if ((clientSock = accept(serverSock, &clientAddr, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            const std::string clientIpStr = ipToStr(&clientAddr);
            try {
                handleClient(clientSock, clientAddr);
            } catch (const ClientDisconnected&) {
                std::cout << "(" << clientIpStr << ") - Client disconnected" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "(" << clientIpStr << ") - " << e.what() << std::endl;
            }

            try {
                tryClose(clientSock, "Cannot close client socket");
            } catch (const std::exception& e){
                std::cerr << "(" << clientIpStr << ") - " << e.what() << std::endl;
            }
        }
    }

    void handleClient(int clientSock, const sockaddr& clientAddr) {
        std::cout << "Client connected: " << ipToStr(&clientAddr) << std::endl;
        sendMetadata(clientSock);

        while (true) {
            u_int64_t requestedChunk = receiveChunkReq(clientSock);

            lseek(dataFd, CHUNK_SIZE * requestedChunk, SEEK_SET);
            uint8_t buf[BUF_SIZE];

            u_int64_t readBytes{0};
            while (readBytes < getSizeOfChunk(dataSize, requestedChunk, CHUNK_SIZE)) {
                ssize_t rv = read(dataFd, buf, BUF_SIZE);
                if (rv < 0) {
                    perror("read");
                    throw std::runtime_error("Cannot read requested chunk");
                } else if (rv == 0) {
                    break;
                }

                readBytes += rv;
                try {
                    tryWriteAll(clientSock, buf, static_cast<size_t>(rv));
                } catch (const std::exception& e) {
                    throw std::runtime_error("Cannot send requested chunk");
                }
            }
        }
    }

    void sendMetadata(int clientSock) {
        MsgMetadata msg(base_name(filepath), static_cast<u_int64_t>(dataSize));
        tryWriteAll(clientSock, msg.generateMsg(), MsgMetadata::MSG_SIZE);
    }

    u_int64_t receiveChunkReq(int clientSock) {
        u_int64_t requestedChunk;
        ssize_t r = read(clientSock, &requestedChunk, sizeof(requestedChunk));
        if (r == -1) {
            perror("read");
            throw std::runtime_error("Cannot receive chunk req");
        } else if (r == 0) {
            throw ClientDisconnected();
        }

        if (requestedChunk > chunks)
            throw std::runtime_error("Invalid chunk requested. Dropping connection");

        std::cout << "Chunk " << requestedChunk << " requested"<< std::endl;
        return requestedChunk;
    }

    std::string get_data_path() const {
        return filepath + ".gzip";
    }

    void load_settings(int argc, char **argv) {
        if (argc < 2) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }

        filepath = argv[1];
        if (argc == 3)
            port = argv[2];
    }

    void validate_settings() {
        off_t fileSize = getFileSize(filepath);
        std::cout << "Provided file has " << fileSize << " bytes" << std::endl;
    }

    void print_usage(const char *name) {
        std::cout << "Usage: " << name << " <filepath> <port>" << std::endl;
    }

    std::string base_name(const std::string &path) {
        return path.substr(path.find_last_of('/') + 1);
    }

    struct ClientDisconnected : std::exception {};

    const int COMPRESSION_LEVEL{6};
    const size_t BUF_SIZE{8192};

    std::string filepath;
    std::string port = "8000";
    u_int64_t chunks;
    u_int64_t dataSize;
    int serverSock;
    int dataFd;
};
