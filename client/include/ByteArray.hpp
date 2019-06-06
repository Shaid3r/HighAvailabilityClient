#pragma once


#include <zconf.h>

constexpr int BUF_SIZE{8192};

struct ByteArray {
    u_int8_t value[BUF_SIZE];
};