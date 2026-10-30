#pragma once
#include "tpx3_helper.hpp"
#include "Batch/Batch.hpp"
#include "Batch/Batch_Pool.hpp"

class ParseTrigger {
    Batch_Pool<TriggerBatch> &batch_pool;
    ClockCorrection<ONE_HUNDRED_SECONDS> clock_correction_;

public:
    TriggerBatchPtr parse(const U64BatchPtr &batch) {
        TriggerBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        out_batch->sequence_number = batch->sequence_number;
        for (auto u64 : *batch) {
            auto [type, toa] = tpx3_helper::parse_trigger(u64);
            out_batch->push_back(toa, 0, type);
        }
        for (auto & toa : out_batch->toa) {
            toa =clock_correction_.clock(toa);
        }
        return out_batch;
    }

    ParseTrigger(Batch_Pool<TriggerBatch> &t_batch_pool)
        : batch_pool(t_batch_pool) {}
};
