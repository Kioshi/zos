#pragma once
#include <stdint.h>
#include <mutex>
#include <string>
#include <cstddef>

typedef uint64_t uint64;
typedef uint32_t uint32;
typedef int32_t int32;
typedef uint16_t uint16;
typedef int16_t int16;
typedef uint8_t uint8;
typedef int8_t int8;

typedef std::unique_lock<std::mutex> Guard;

enum
{
    THREADS = 3,
    RANDOM_RANGE = 100
};

