#pragma once
// #include <getopt.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include <fcntl.h>
#include <netdb.h>
#include "gzip.hpp"
#include "utils.hpp"
#include "proto.hpp"
#include "Downloader.hpp"


class Client {
public:
    Client(int argc, char **argv) {
        std::cout << "Loading settings" << std::endl;
        load_settings(argc, argv);
        downloadOrchestrator.start();
        // start();
        // downloadChunks
        // mergeChunks
        // decompress

        //connect ignore EINPROGRESS Then you call select() with whatever timeout you want, passing the socket descriptor in both the read and write sets. If it doesn't timeout, it means the connect() call completed. At this point, you'll have to use getsockopt() with the SO_ERROR option to get the return value from the connect() call, which should be zero if there was no error.
        // ECONNREFUSED when listen queue is full

        // vector<hostname, port, sockfd>

        // idx - chunk_nr - vector<int> first_server_nr_that_downloaded 
        // getChunkNumberToDownload() { next = 0; ++next, for (int i = 0; i < MAX; i++) if downloaded[i] == -1}
        // workerPool
    }

    ~Client() {
        std::cout << "Deleting client" << std::endl;
    }


private:
//    void start() {
//        const char *PORT = "8000";
//
//        struct addrinfo hints, *serverInfo;
//        memset(&hints, 0, sizeof(hints));
//        hints.ai_family = AF_UNSPEC;
//        hints.ai_socktype = SOCK_STREAM;
//        int rv;
//        if ((rv = getaddrinfo(NULL, PORT, &hints, &serverInfo)) != 0) {
//            fprintf(stderr, "getaddinfo: %s\n", gai_strerror(rv));
//            exit(1);
//        }
//
//        int serverSock;
//        struct addrinfo *rp;
//        for (rp = serverInfo; rp != NULL; rp = rp->ai_next) {
//            if ((serverSock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) {
//                perror("socket");
//                continue;
//            }
//
//            std::cout << "Connecting to: " << ip_to_str(rp->ai_addr);
//            if (connect(serverSock, rp->ai_addr, rp->ai_addrlen) != -1) {
//                std::cout << "\t\tconnected" << std::endl;
//                break;  /* Success */
//            }
//            std::cout << std::endl;
//            perror("connect");
//            close(serverSock);
//        }
//
//        if (rp == NULL) {   /* No address succeeded */
//            fprintf(stderr, "Could not connect\n");
//            exit(EXIT_FAILURE);
//        }
//
//        freeaddrinfo(serverInfo);
//
//        Proto proto(serverSock); // receive metadata
//        std::cout << "fn '"<< proto.filename << "' bytes: " << proto.filesize << std::endl;
//        lastChunkSize = proto.filesize % CHUNK_SIZE;
//        chunks = getNumberOfChunks(proto.filesize, CHUNK_SIZE);
//
//        u_int64_t chunk_nr = 2;
//        write(serverSock, &chunk_nr, sizeof(chunk_nr));
//
//        if (mkdir("workspace", S_IRWXU) != 0) {
//            printf("Cannot create workspace directory");
//            perror("mkdir");
//            return;
//        }
//
//        int chunkfd = open("workspace/chunk2", O_CREAT|O_WRONLY|O_EXCL);
//
//        char buf[8000];
//
//        int read_bytes = 0;
//        while (read_bytes < CHUNK_SIZE) {
//            int ret = read(serverSock, buf, 8000);
//            std::cout << "Received " << ret << " bytes" << std::endl;
//
//            if (rv < 0) {
//                perror("receive data");
//                exit(-1);
//            } else if (rv == 0) {
//                std::cout << "No data" << std::endl;
//                // break;
//            }
//
//            int wb = write(chunkfd, buf, ret);
//            std::cout << "Wrote " << wb << " bytes" << std::endl;
//            read_bytes += ret;
//        }
//
//        std::cout << "Disconnecting" << std::endl;
//
//        close(chunkfd);
//        close(serverSock);
//    }

    
    void load_settings(int argc, char **argv) {
        if (argc < 2) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        
        for (int i = 1; i < argc; ++i)
            add_server(argv[i]);
    }

    void add_server(const std::string& serverAddrInfo) {
        port_t port("8000");
        std::size_t pos = serverAddrInfo.find_first_of(':');
        if (pos != std::string::npos) 
            port = serverAddrInfo.substr(pos + 1);
//        servers[serverAddrInfo.substr(0, pos)] = port;
        std::cout << "Adding server: " << serverAddrInfo.substr(0, pos) << ":" << port << std::endl;
        downloadOrchestrator.addServer(serverAddrInfo.substr(0, pos), port);
    }

    void print_usage(const char *name) const {
        std::cout << "Usage: " << name << " <server>:<port>" << std::endl
            << "Example: " << name << " localhost:8080" << std::endl;
    }

    using hostname_t = std::string;
    using port_t = std::string;
//    std::unordered_map<hostname_t, port_t> servers;

    Downloader downloadOrchestrator;
  
    std::string filepath;
    unsigned int chunks;
    unsigned int lastChunkSize;
};