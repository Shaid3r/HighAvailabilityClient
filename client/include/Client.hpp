#pragma once
#include <iostream>
#include <unordered_map>
#include "Downloader.hpp"
#include "Gzip.hpp"
#include "utils.hpp"


class Client {
public:
    Client(int argc, char **argv) {
        std::cout << "Loading settings" << std::endl;
        load_settings(argc, argv);
        try {
            const auto& savedChunks = downloader.downloadChunks();
            mergeChunks(savedChunks);
            std::cout << "Chunks merged to " << MERGED_FILENAME << ". Decompressing..." << std::endl;
            Gzip::decompress(MERGED_FILENAME, "rr");
            std::cout << "Done. Cleaning..." << std::endl;
            removeRecursively("workspace");
//            removeRecursively(MERGED_FILENAME);
            std::cout << "============================================" << std::endl;
            std::cout << "File download completed!!!" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    ~Client() {
        std::cout << "Deleting client" << std::endl;
    }

private:
    void mergeChunks(const std::unordered_map<uint64_t, std::string> &savedChunks) {
        int mergedFd = open(MERGED_FILENAME, O_CREAT | O_WRONLY | O_TRUNC, S_IRWXU);
        if (mergedFd == -1) {
            perror("open");
            throw std::exception();
        }

        for (const auto& chunk : savedChunks) {
            int chunkFd = open(chunk.second.c_str(), O_RDONLY);
            if (chunkFd == -1) {
                perror("open");
                throw std::exception();
            }

            char buf[BUF_SIZE];
            off_t chunkSize = getFileSize(chunk.second);

            off_t writtenBytes = 0;
            while (writtenBytes < chunkSize) {
                ssize_t readBytes = read(chunkFd, buf, BUF_SIZE);
                if (readBytes < 0) {
                    perror("open");
                    throw std::runtime_error("Cannot read: " + chunk.second);
                }

                writeAll(mergedFd, buf, static_cast<size_t>(readBytes));
                writtenBytes += readBytes;
            }
            if (close(chunkFd)) {
                perror("close");
                throw std::runtime_error("Cannot close: " + chunk.second);
            }
        }

        if (close(mergedFd)) {
            perror("close");
            throw std::runtime_error("Cannot close merged file");
        }
    }

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
        downloader.addServer(serverAddrInfo.substr(0, pos), port);
    }

    void print_usage(const char *name) const {
        std::cout << "Usage: " << name << " <server>:<port>" << std::endl
            << "Example: " << name << " localhost:8080" << std::endl;
    }

    const char *MERGED_FILENAME = "mergedFile";

    using hostname_t = std::string;
    using port_t = std::string;
    Downloader downloader;
};