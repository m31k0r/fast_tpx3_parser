#pragma once
#include "clock_correction.hpp"
#include "Rotation.hpp"
#include "Batch/Batch_Pool.hpp"
#include "Camera.hpp"

class ParseHit {
    Batch_Pool<HitBatch> &batch_pool;
    ClockCorrection<TWENTY_SIXISH_SECONDS> clock_correction_;
uint8_t chip_nr;

public:
    HitBatchPtr parse(U64BatchPtr batch) {
        HitBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        out_batch->sequence_number = batch->sequence_number;
        for (auto u64: *batch) {
            auto hit = tpx3_helper::Hit(u64);
            auto [x_filled, y_filled] =
                  camera::fill_gap({hit.x, hit.y}, chip_nr);

            auto [x, y] =
                camera::to_global({x_filled, y_filled}, chip_nr);

            hit.toa = clock_correction_.clock(hit.toa - camera::get_row_correction(
                                               chip_nr, hit.x));


            out_batch->toa[out_batch->size] = clock_correction_.clock(hit.get_toa()+camera::get_row_correction(chip_nr,hit.x));
            out_batch->tot[out_batch->size] = hit.tot;
            out_batch->x[out_batch->size] = x;
            out_batch->y[out_batch->size] = y;
            out_batch->size++;
        }
        return out_batch;
    }

    ParseHit(Batch_Pool<HitBatch> &t_batch_pool) : batch_pool(t_batch_pool) {
    }
};
