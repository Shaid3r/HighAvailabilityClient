// #include <getopt.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include "Gzip.hpp"
#include "utils.hpp"
#include "proto.hpp"

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
        close(datafd);
        close(serverSock);
    }

private:
    void initSocket() {
        struct addrinfo hints, *serverInfo;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags = AI_PASSIVE;
        int rv;
        if ((rv = getaddrinfo(NULL, port.c_str(), &hints, &serverInfo)) != 0) {
            fprintf(stderr, "getaddinfo: %s\n", gai_strerror(rv));
            exit(1);
        }

        struct addrinfo *rp;
        for (rp = serverInfo; rp != NULL; rp = rp->ai_next) {
            if ((serverSock = socket(rp->ai_family, rp->ai_socktype,
                                     rp->ai_protocol)) == -1) {
                perror("socket");
                continue;
            }

            int enable = 1;
            if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                           &enable, sizeof(enable)) == -1) {
                perror("setsockopt");
                exit(EXIT_FAILURE);
            }

            // if (fcntl(serverSock, F_SETFL, O_NONBLOCK) == -1) {
            //     perror("fcntl");
            //     exit(EXIT_FAILURE);
            // }

            if (bind(serverSock, rp->ai_addr, rp->ai_addrlen) == 0)
                break;
            perror("bind");
        }

        if (rp == NULL) {
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
        struct sockaddr clientAddr;
        socklen_t addrlen = sizeof(clientAddr);

        while (true) {
            if ((clientSock = accept(serverSock, &clientAddr, &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            std::cout << "Client connected: " << ip_to_str(&clientAddr) << std::endl;
            Proto proto(base_name(filepath), dataSize);
            ssize_t wv = write(clientSock, proto.raw_msg(), proto.size());
            std::cout << "Send metadata: " << wv << " bytes" << std::endl;

            bool clientConnected = true;
            while (clientConnected) {
                // Receive chunk request
                u_int64_t requestedChunk;
                ssize_t r = read(clientSock, &requestedChunk, sizeof(requestedChunk));
                if (r == -1) {
                    perror("read");
                    clientConnected = false;
                    continue;
                }
                if (requestedChunk > chunks) {
                    std::cerr << "Invalid chunk requested. Dropping connection"
                              << std::endl;
                    clientConnected = false;
                    continue;
                }
                std::cout << "Chunk " << requestedChunk << " requested"<< std::endl;

                lseek(datafd, CHUNK_SIZE * requestedChunk, SEEK_SET);

                size_t BUF_SIZE = 8192;
                uint8_t buf[BUF_SIZE];

                int read_bytes = 0;
                std::cout << "size: " << getSizeOfChunk(0) << std::endl;
                while (read_bytes < getSizeOfChunk(requestedChunk)) {
                    ssize_t rv = read(datafd, buf, BUF_SIZE);
                    if (rv < 0) {
                        perror("read data");
                        exit(-1);
                    } else if (rv == 0) {
                        clientConnected = false;
                    }

                    read_bytes += rv;
                    try {
                        writeAll(clientSock, buf, rv);
                    } catch (const std::exception& e) {
                        clientConnected = false;
                        break;
                    }
                }
            }

            std::cout << "Client disconnected" << std::endl;
            if (close(clientSock)) {
                perror("close");
            }
        }
    }
    u_int64_t getSizeOfChunk(u_int64_t chunkNo) const {
        if (chunkNo < getNumberOfChunks(dataSize, CHUNK_SIZE) - 1)
            return CHUNK_SIZE;
        return dataSize % CHUNK_SIZE;
    }

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
        datafd = open(data_path.c_str(), O_RDONLY, 0);
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
        // wait for conn

        // const option long_options[] = {
        //     {"add",     required_argument, 0,  0 },
        //     {"append",  no_argument,       0,  0 },
        //     {"delete",  required_argument, 0,  0 },
        //     {"verbose", no_argument,       0,  0 },
        //     {"create",  required_argument, 0, 'c'},
        //     {"file",    required_argument, 0,  0 },
        //     {0,         0,                 0,  0 }
        // };
    }

    void validate_settings() {
        off_t fileSize = getFileSize(filepath);
        std::cout << "Provided file has " << fileSize << " bytes" << std::endl;
    }

    void print_usage(const char *name) {
        std::cout << "Usage: " << name << " <filepath>" << std::endl;
    }

    std::string base_name(const std::string &path) {
        return path.substr(path.find_last_of('/') + 1);
    }

//    const char *PORT = "8000";
    const int COMPRESSION_LEVEL{6};

    std::string filepath;
    std::string port = "8000";
    off_t chunks;
    off_t dataSize;
    int serverSock;
    int datafd;
};
