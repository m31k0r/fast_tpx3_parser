#pragma once
#include <cstdint>
#include "Batch/Batch.hpp"
#include "Batch/Batch_Pool.hpp"
#include "Camera.hpp"
#include "Parsing/tpx3_helper.hpp"
#include "Rotation.hpp"
#include "clock_correction.hpp"

struct PreParsed {
    uint64_t data;
    uint64_t toa;
};
class ParseHit {
    Batch_Pool<HitBatch> &batch_pool;
    ClockCorrection<TWENTY_SIXISH_SECONDS> clock_correction_;
    uint8_t chip_nr;
    Camera &camera;

   public:
    PreParsed parse(uint64_t t_data) {
        auto pix_addr = get_bits(t_data, 44, 59);
        auto toa = get_bits(t_data, 30, 43);
        auto f_toa = get_bits(t_data, 16, 19);
        auto spidr = get_bits(t_data, 0, 15);

        auto dcol = (pix_addr & 0x0FE00) >> 8;
        auto pix = pix_addr & 0x00007;
        auto x = (dcol + (pix / 4));

        auto count =
            ((((static_cast<uint64_t>(spidr) << 14) + toa) << 4) - f_toa);
        constexpr uint64_t clock_factor = 1562;
        toa = (count * clock_factor) + (count / 2);
        toa = clock_correction_.clock(toa -
                                      camera.get_row_correction(chip_nr, x));
        return {t_data, toa};
    }

    HitBatchPtr parse(U64BatchPtr &batch) {
        HitBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        out_batch->sequence_number = batch->sequence_number;
        for (auto u64 : *batch) {
            auto hit = tpx3_helper::Hit(u64);
            auto [x_filled, y_filled] =
                camera.fill_gap({hit.x, hit.y}, chip_nr);

            auto [x, y] = camera.to_global({x_filled, y_filled}, chip_nr);

            out_batch->toa[out_batch->size] = clock_correction_.clock(
                hit.get_toa() + camera.get_row_correction(chip_nr, hit.x));
            out_batch->tot[out_batch->size] = hit.tot;
            out_batch->x[out_batch->size] = x;
            out_batch->y[out_batch->size] = y;
            out_batch->size++;
        }
        return out_batch;
    }

    HitBatchPtr parse_no_toa(PreParsed *begin, PreParsed *end) {
        HitBatchPtr out_batch = batch_pool.acquire_batch_blocking();
        for (; begin < end; begin = std::next(begin)) {
            uint64_t t_data = begin->data;
            auto pix_addr = get_bits(t_data, 44, 59);
            auto tot = get_bits(t_data, 20, 29);

            auto dcol = (pix_addr & 0x0FE00) >> 8;
            // superpixel * 4
            auto spix = (pix_addr & 0x001F8) >> 1;  // (16+28+3-2)
            // pixel
            auto pix = pix_addr & 0x00007;
            auto x = (dcol + (pix / 4));
            auto y = (spix + (pix & 0x3));
            // NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
            auto [x_filled, y_filled] = camera.fill_gap({x, y}, chip_nr);

            auto [x_final, y_final] =
                camera.to_global({x_filled, y_filled}, chip_nr);

            out_batch->tot[out_batch->size] = tot;
            out_batch->x[out_batch->size] = x_final;
            out_batch->y[out_batch->size] = y_final;
            out_batch->size++;
        }
        return out_batch;
    }
    ParseHit(Batch_Pool<HitBatch> &t_batch_pool, Camera &camera,
             uint8_t chip = 0)
        : batch_pool(t_batch_pool), chip_nr(chip), camera(camera) {}
};
