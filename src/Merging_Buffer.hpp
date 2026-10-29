#pragma once
#include <sys/types.h>
#include <assert.hpp>
#include <cstdint>
#include <stop_token>
#include "Batch.hpp"
#include "Constants.hpp"
#include "Extract_Queue.hpp"
#include "SPSCQueue.h"

class Merger {
    std::array<Extractor<rigtorp::SPSCQueue<HitBatchPtr>, HitBatchPtr>, 4>
        extractors;

    HitBatchPtr current_batch;
    Batch_Pool<HitBatch>& batch_pool;
    void fill_batch() {
        DEBUG_ASSERT(not current_batch);
        current_batch = batch_pool.acquire_batch_blocking();

        for (size_t i = 0; i < BATCH_SIZE; ++i) {
            uint64_t earliest_time = std::numeric_limits<uint64_t>::max();
            uint8_t earliest_chip = 0;
            for (int i = 0; i < 4; ++i) {
                adaptive_backoff backoff;
                while (not extractors[i].front()) {
                    backoff.pause();
                    if (stop.stop_requested()) {
                        return;
                    }
                }
                auto hit = extractors[i].front();
                if (hit.toa() < earliest_time) {
                    earliest_time = hit.toa();
                    earliest_chip = i;
                }
            }
            const auto hit = extractors[earliest_chip].front();
            current_batch->push_back(hit.toa(), hit.tot(), hit.x(), hit.y());
            extractors[earliest_chip].pop();
        }
    };

   public:
    std::stop_token stop;
    using Array =
        std::array<Extractor<rigtorp::SPSCQueue<HitBatchPtr>, HitBatchPtr>, 4>;
    Merger(Batch_Pool<HitBatch>& t_batch_pool, Array arr,
           std::stop_token t_stop = {})
        : extractors(std::move(arr)), batch_pool(t_batch_pool), stop(t_stop) {};
    void set_stop_token(std::stop_token t_stop) { stop = t_stop; }
    HitBatchPtr* front() {
        if (not current_batch) fill_batch();
        return &current_batch;
    };
    void pop() { current_batch = nullptr; };
};
