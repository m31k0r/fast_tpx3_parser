#pragma once
#include "tpx3_helper.hpp"
#include "Batch/Batch.hpp"
#include "Batch/Batch_Pool.hpp"

class ParseTrigger {
    Batch_Pool<TriggerBatch> &batch_pool;

public:
    TriggerBatchPtr parse(U64BatchPtr batch) {
        TriggerBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        out_batch->sequence_number = batch->sequence_number;
        for (auto u64 : *batch) {
            auto trigger = tpx3_helper::Trigger(u64);
            out_batch->push_back(trigger.get_toa(), 0, trigger.get_type());
        }
        return out_batch;
    }

    ParseTrigger(Batch_Pool<TriggerBatch> &t_batch_pool)
        : batch_pool(t_batch_pool) {}
};
