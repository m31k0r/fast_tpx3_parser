#pragma once
#include "clock_correction.hpp"
#include "Rotation.hpp"
#include "Batch/Batch_Pool.hpp"

class ParseHit {
    Batch_Pool<HitBatch> &batch_pool;
    ClockCorrection<TWENTY_SIXISH_SECONDS> clock_correction_;
    Rotation rotation_;

public:
    HitBatchPtr parse(U64BatchPtr batch) {
        HitBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        out_batch->sequence_number = batch->sequence_number;
        for (auto u64: *batch) {
            auto hit = tpx3_helper::Hit(u64);
            out_batch->toa[out_batch->size] = clock_correction_.clock(hit.get_toa());
            out_batch->tot[out_batch->size] = hit.tot;
            auto [x,y] = rotation_.rotate(hit.x,hit.y);
            out_batch->x[out_batch->size] = x;
            out_batch->y[out_batch->size] = y;
            out_batch->size++;
        }
        return out_batch;
    }

    ParseHit(Batch_Pool<HitBatch> &t_batch_pool, const std::string &rotation="LtRBtT") : batch_pool(t_batch_pool),
                                                                         rotation_(Rotation::from_string(rotation)) {
    }
};
