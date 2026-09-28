#include <thread>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Constants.hpp"
#include "mpmc_queue.h"
#include "tpx3_helper.hpp"

class Hit_Consumer {
    std::jthread m_thread;

   public:
    void operator()(
        es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &queue,
        Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool,
        std::stop_token stop) {
        Batch<uint64_t> *batch;
        while (not stop.stop_requested()) {
            if (queue.pop(batch)) {
                // for (size_t i = 0; i < batch->insert_idx; ++i) {
                //     auto data = batch->data[i];
                //     auto hit = tpx3_helper::Hit(data);
                //     asm volatile("" ::"g"(hit) : "memory");
                // }
                batch_pool.release_batch_blocking(batch);
            }
        }
    }
    void start(es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &queue,
               Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool) {
        m_thread =
            std::jthread([this, &queue, &batch_pool](std::stop_token stop) {
                (*this)(queue, batch_pool, stop);
            });
    }
    void stop() { m_thread.request_stop(); }
};

class Trigger_Consumer {
    std::jthread m_thread;

   public:
    void operator()(
        es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &queue,
        Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool,
        std::stop_token stop) {
        Batch<uint64_t> *batch;
        while (not stop.stop_requested()) {
            if (queue.pop(batch)) {
                // for (size_t i = 0; i < batch->insert_idx; ++i) {
                //     auto data = batch->data[i];
                //     auto trigger = tpx3_helper::Trigger(data);
                //     asm volatile("" ::"g"(trigger) : "memory");
                // }
                //
                batch_pool.release_batch_blocking(batch);
            }
        }
    }
    void start(es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &queue,
               Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool) {
        m_thread =
            std::jthread([this, &queue, &batch_pool](std::stop_token stop) {
                (*this)(queue, batch_pool, stop);
            });
    }
    void stop() { m_thread.request_stop(); }
};
