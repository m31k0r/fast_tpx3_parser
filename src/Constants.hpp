#pragma once

#include <algorithm>
#include <assert.hpp>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <new>
#include <thread>

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

constexpr auto num_runs = 30;
constexpr size_t BATCH_SIZE = std::hardware_constructive_interference_size * 16;
constexpr size_t BATCH_POOL_SIZE = 32 * 32;
constexpr size_t QUEUE_SIZE = 16;


