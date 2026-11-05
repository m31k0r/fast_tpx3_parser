#pragma once
#include <sys/types.h>
#include <algorithm>
#include <assert.hpp>
#include <cstdint>
#include <stop_token>
#include <utility>
#include "../Batch/Batch.hpp"
#include "../Constants.hpp"
#include "../QueueUtility/Extractor.hpp"
#include "../Utility/SPSCQueue.h"
#include "Utility/AdaptivBackoff.hpp"

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

            for (int chip_nr = 0; chip_nr < 4; ++chip_nr) {
                adaptive_backoff backoff;
                while (not extractors[chip_nr].front()) {
                    backoff.pause();
                    if (stop.stop_requested()) {
                        return;
                    }
                }
                auto hit = extractors[chip_nr].front();
                if (hit.toa() < earliest_time) {
                    earliest_time = hit.toa();
                    earliest_chip = chip_nr;
                }
            }
            const auto hit = extractors[earliest_chip].front();
            current_batch->push_back({hit.toa(), hit.tot(), hit.x(), hit.y()});
            extractors[earliest_chip].pop();
        }
    };

   public:
    std::stop_token stop;
    using Array =
        std::array<Extractor<rigtorp::SPSCQueue<HitBatchPtr>, HitBatchPtr>, 4>;
    Merger(Batch_Pool<HitBatch>& t_batch_pool, Array arr,
           std::stop_token t_stop = {})
        : extractors(std::move(arr)),
          batch_pool(t_batch_pool),
          stop(std::move(t_stop)) {};
    void set_stop_token(const std::stop_token& t_stop) { stop = t_stop; }
    HitBatchPtr* front() {
        if (not current_batch) fill_batch();
        return &current_batch;
    };
    void pop() { current_batch = nullptr; };
    size_t size() {
        auto min = std::min_element(
            extractors.begin(), extractors.end(),
            [](auto& e1, auto& e2) { return e1.size() < e2.size(); });
        return min->size();
    }
};
