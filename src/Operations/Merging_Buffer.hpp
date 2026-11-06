#pragma once
#include <sys/types.h>
#include <algorithm>
#include <array>
#include <assert.hpp>
#include <cstdint>
#include <stop_token>
#include <utility>
#include "Batch/Batch.hpp"
#include "Constants.hpp"
#include "QueueUtility/Extractor.hpp"
#include "Utility/SPSCQueue.h"

class Merger {
    std::array<Extractor<rigtorp::SPSCQueue<HitBatchPtr>, HitBatchPtr>, 4>
        extractors;

    HitBatchPtr current_batch;
    Batch_Pool<HitBatch>& batch_pool;

    int next_time() {
        uint64_t earliest_time = std::numeric_limits<uint64_t>::max();
        int8_t earliest_chip = -1;
        for (int chip_nr = 0; chip_nr < 4; ++chip_nr) {
            if (not extractors[chip_nr].front()) {
                // Keep other producers from blocking on full queues
                if (extractors[chip_nr].flush) continue;
                return -1;
            }
            auto hit = extractors[chip_nr].front();
            if (hit.toa() < earliest_time) {
                earliest_time = hit.toa();
                earliest_chip = chip_nr;
            }
        }
        return earliest_chip;
    }
    bool fill_batch() {
        if (not current_batch)
            current_batch = batch_pool.acquire_batch_blocking();
        while (current_batch->size < BATCH_SIZE) {
            for (auto& e : extractors) e.drain();
            int8_t chip_nr = next_time();
            if (chip_nr == -1) return false;
            const auto hit = extractors[chip_nr].front();
            current_batch->push_back({hit.toa(), hit.tot(), hit.x(), hit.y()});
            extractors[chip_nr].pop();
        }
        return true;
    };

   public:
    std::stop_token stop;
    using Array =
        std::array<Extractor<rigtorp::SPSCQueue<HitBatchPtr>, HitBatchPtr>, 4>;
    Merger(Batch_Pool<HitBatch>& t_batch_pool, Array arr,
           std::stop_token t_stop = {})
        : extractors(std::move(arr)),
          batch_pool(t_batch_pool),
          stop(std::move(t_stop)){};
    void set_stop_token(const std::stop_token& t_stop) { stop = t_stop; }
    HitBatchPtr* front() {
        if (fill_batch()) return &current_batch;
        return nullptr;
    };
    void pop() { current_batch = nullptr; };
    size_t size() {
        auto min = std::min_element(
            extractors.begin(), extractors.end(),
            [](auto& e1, auto& e2) { return e1.size() < e2.size(); });
        return min->size();
    }
};
