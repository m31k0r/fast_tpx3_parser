#pragma once

#include <cstddef>
#include <new>

constexpr size_t BATCH_SIZE =
    std::hardware_constructive_interference_size / 16 * 256;
constexpr size_t BATCH_POOL_SIZE = 32 * 32;
constexpr size_t QUEUE_SIZE = 4;
