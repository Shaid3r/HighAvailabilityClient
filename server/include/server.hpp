// #include <getopt.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>
#include "gzip.hpp"
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
        if ((rv = getaddrinfo(NULL, PORT, &hints, &serverInfo)) != 0) {
            fprintf(stderr, "getaddinfo: %s\n", gai_strerror(rv));
            exit(1);
        }

        struct addrinfo *rp;
        for (rp = serverInfo; rp != NULL; rp = rp->ai_next) {
            if ((serverSock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
                perror("socket");
                continue;
            }

            int enable = 1;
            if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &enable, sizeof(enable)) == -1) {
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
            if ((clientSock = accept(serverSock, (sockaddr *) &clientAddr,
                                     &addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            std::cout << "Client connected: " << ip_to_str(&clientAddr)
                      << std::endl;

            std::cout << "DEBUG: " << clientSock << std::endl;

            Proto proto("dd.tar", dataSize);
            int wv = write(clientSock, proto.raw_msg(), proto.size());
            std::cout << "Send metadata: " << wv << " bytes" << std::endl;

            u_int64_t requestedChunk = -1;
            read(clientSock, &requestedChunk, sizeof(requestedChunk));
            if (requestedChunk > chunks) {
                std::cerr << "Invalid chunk requested. Dropping connection" << std::endl;
                close(clientSock);
                continue;
            }
            std::cout << "Chunk " << requestedChunk << " requested"
                      << std::endl;

            lseek(datafd, CHUNK_SIZE * (requestedChunk - 1), SEEK_SET);

            int BUF_SIZE = 8192;
            char buf[BUF_SIZE];


            int read_bytes = 0;
            while (read_bytes <= CHUNK_SIZE) {
                int rv = read(datafd, buf, BUF_SIZE);
                if (rv < 0) {
                    perror("read data");
                    exit(-1);
                } else if (rv == 0) {
                    break;
                }

                read_bytes += rv;

                int send_bytes = 0;
                // while (send_bytes < )
                int written_bytes = write(clientSock, buf, rv);

                if (written_bytes == -1) {
                    perror("write");
                    break;
                }
                std::cout << "Send: " << written_bytes << " Requested: " << rv
                          << " Total: " << read_bytes << std::endl;
            }

            std::cout << "Client disconnected" << std::endl;
            close(clientSock);
        }
    }

    void prepare_data() {
        const std::string data_path = get_data_path();
        if (doesFileExists(data_path)) {
            std::cout << "Compressed data (" << data_path << ") already exists." << std::endl;
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
        std::cout << "Compressed data has " << dataSize << " bytes (" << chunks << " chunks)" << std::endl;
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

    const char *PORT = "8000";
    const int COMPRESSION_LEVEL{6};

    std::string filepath;
    unsigned int chunks;
    off_t dataSize;
    int serverSock;
    int datafd;
};
