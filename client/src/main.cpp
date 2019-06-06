#include "Client.hpp"

int main(int argc, char **argv) {
    try {
        Client client(argc, argv);
    } catch (const std::exception& e) {
        std::cout << "EXCEPTION: " << e.what() << std::endl;
    }
}
