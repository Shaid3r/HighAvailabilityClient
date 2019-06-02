cmake_minimum_required (VERSION 2.4)

project(subprojects)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -Wall -Wextra")

add_subdirectory(server)
add_subdirectory(client)
add_subdirectory(common)