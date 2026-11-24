#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include "Batch/Batch.hpp"
#include "Constants.hpp"
#include "QueueUtility/Extractor.hpp"
#include "QueueUtility/QueueMessage.hpp"
#include "Utility/SPSCQueue.h"

class Merger {
   public:
    using HitQueue = rigtorp::SPSCQueue<QueueMessage<HitBatchPtr>>;
    using Array = std::array<Extractor<HitQueue, HitBatchPtr>, 4>;

   private:
    Array extractors;
    HitBatchPtr current_batch;
    std::optional<QueueMessage<HitBatchPtr>> current;
    Batch_Pool<HitBatch>& batch_pool;
    bool eof_emitted = false;

    int next_time() {
        uint64_t earliest_time = std::numeric_limits<uint64_t>::max();
        int earliest_chip = -1;
        for (int chip = 0; chip < 4; ++chip) {
            auto hit = extractors[chip].front();
            if (not hit) {
                if (extractors[chip].eof() || extractors[chip].new_file())
                    continue;
                return -1;
            }
            if (earliest_chip == -1 || hit.toa() < earliest_time) {
                earliest_time = hit.toa();
                earliest_chip = chip;
            }
        }
        return earliest_chip;
    }

    bool emit_boundary() {
        const bool all_eof =
            std::all_of(extractors.begin(), extractors.end(),
                        [](const auto& e) { return e.eof(); });
        const bool all_new_file =
            std::all_of(extractors.begin(), extractors.end(),
                        [](const auto& e) { return e.new_file(); });
        const bool all_at_boundary =
            std::all_of(extractors.begin(), extractors.end(),
                        [](const auto& e) { return e.eof() || e.new_file(); });
        ASSERT(not all_at_boundary || all_eof || all_new_file,
               "Mismatched hit-stream boundaries");
        if (not all_eof && not all_new_file) return false;
        if (current_batch->size) {
            current.emplace(std::move(current_batch));
            return true;
        }
        current_batch.reset();
        if (all_eof) {
            current.emplace(EndOfStream{});
            eof_emitted = true;
        } else {
            for (size_t i = 1; i < extractors.size(); ++i) {
                ASSERT(extractors[i].file_message().filename ==
                           extractors[0].file_message().filename,
                       "Mismatched new-file boundaries");
            }
            current.emplace(extractors.front().file_message());
            for (auto& extractor : extractors)
                extractor.consume_new_file();
        }
        return true;
    }

   public:
    Merger(Batch_Pool<HitBatch>& pool, Array arr)
        : extractors(std::move(arr)), batch_pool(pool) {}

    QueueMessage<HitBatchPtr>* front() {
        if (current) return &*current;
        if (eof_emitted) return nullptr;
        if (not current_batch) current_batch = batch_pool.acquire_batch_blocking();
        while (current_batch->size < BATCH_SIZE) {
            for (auto& extractor : extractors) extractor.drain();
            if (emit_boundary()) return &*current;
            const int chip = next_time();
            if (chip == -1) {
                if (emit_boundary()) return &*current;
                return nullptr;
            }
            const auto hit = extractors[chip].front();
            current_batch->push_back({hit.toa(), hit.tot(), hit.x(), hit.y()});
            extractors[chip].pop();
        }
        current.emplace(std::move(current_batch));
        return &*current;
    }
    void pop() { current.reset(); }
    size_t size() {
        size_t count = current ? 1 : 0;
        for (auto& extractor : extractors) count += extractor.size();
        return count;
    }
};
