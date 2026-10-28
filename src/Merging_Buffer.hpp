#pragma once
#include <sys/types.h>
#include <assert.hpp>
#include <cstdint>
#include "Batch.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "Parser.hpp"
#include "QueueConcepts.hpp"
#include "SPSCQueue.h"

#include <iostream>
#include <print>
#include <ratio>
#include <thread>

class Merger {
    std::array<Extractor<rigtorp::SPSCQueue<HitBatchPtr>, HitBatchPtr>, 4>
        extractors;

    HitBatchPtr current_batch;
    Batch_Pool<HitBatch>& batch_pool;
    void fill_batch() {
        DEBUG_ASSERT(not current_batch);
        current_batch = batch_pool.acquire_batch_blocking();
        uint64_t earliest_time = std::numeric_limits<uint64_t>::max();
        uint8_t earliest_chip = 0;
        for (int i = 0; i < 4; ++i) {
            adaptive_backoff backoff;
            while (not extractors[i].front()) {
                backoff.pause();
            }
            if (extractors[i].front()) {
                auto hit = extractors[i].front();
                if (hit.toa() < earliest_time) {
                    earliest_time = hit.toa();
                    earliest_chip = i;
                }
            }
        }
    };

   public:
    HitBatch* front();
    void pop();
};
