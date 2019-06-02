project(commonlibrary)


#file(GLOB SOURCES "src/*.cpp")
#add_library(${PROJECT_NAME} ${SOURCES})
add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE ${PROJECT_SOURCE_DIR}/include
)

find_package(ZLIB)
target_link_libraries(${PROJECT_NAME} INTERFACE ${ZLIB_LIBRARIES})

add_library(sub::lib1 ALIAS ${PROJECT_NAME})