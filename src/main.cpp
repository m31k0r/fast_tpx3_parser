#include <array>
#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <iterator>
#include <thread>

#include "Memory_Map.hpp"
#include "mpmc_queue.h"
#include "tpx3_helper.hpp"

constexpr size_t BATCH_SIZE = 500;
constexpr size_t BATCH_POOL_SIZE = 128;
constexpr size_t QUEUE_SIZE = 128;

template <typename T>
struct Batch {
    std::array<T, BATCH_SIZE> data{};
    size_t insert_idx{0};
    size_t sequence_number{0};
    uint8_t chip_nr{0};
};

template <typename Batch, size_t BATCH_POOL_SIZE>
class Batch_Pool {
    std::vector<std::unique_ptr<Batch>> m_batches;
    es::lockfree::mpmc_queue<Batch *, BATCH_POOL_SIZE> m_free_batches;

   public:
    Batch_Pool() {
        m_batches.reserve(BATCH_POOL_SIZE);
        for (size_t i = 0; i < BATCH_POOL_SIZE; ++i) {
            auto batch = std::make_unique<Batch>();
            m_free_batches.push(batch.get());
            m_batches.push_back(std::move(batch));
        }
    }
    Batch *acquire_batch() {
        Batch *batch;
        if (m_free_batches.pop(batch)) {
            return batch;
        } else {
            return nullptr;
        }
    }
    bool release_batch(Batch *t_batch) { return m_free_batches.push(t_batch); }
    Batch *acquire_batch_blocking() {
        Batch *batch = nullptr;
        while (not m_free_batches.pop(batch)) {
            //            std::this_thread::sleep_for(std::chrono::nanooseconds(10));
        }
        return batch;
    }
    void release_batch_blocking(Batch *t_batch) {
        while (not m_free_batches.push(t_batch)) {
            //            std::this_thread::sleep_for(std::chrono::nanoseconds(10));
        }
    };
};

class BatchHandle {
   public:
    Batch<uint64_t> *hit_batches[4];
    Batch<uint64_t> *trigger_batch;
    Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> batch_pool;
    size_t m_sequence_number[5] = {0, 0, 0, 0, 0};

    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> m_hit_queue;
    es::lockfree::mpmc_queue<Batch<uint64_t> *, BATCH_POOL_SIZE>
        m_trigger_queue;

    Batch<uint64_t> *new_batch(Batch<uint64_t> *t_old_batch, size_t chip_nr) {
        auto new_batch = batch_pool.acquire_batch_blocking();
        new_batch->insert_idx = 0;
        new_batch->sequence_number = m_sequence_number[chip_nr + 1]++;
        return new_batch;
    }

    void insert_hit(uint8_t chip_nr, uint64_t data) {
        hit_batches[chip_nr]->data[hit_batches[chip_nr]->insert_idx++] = data;
        if (hit_batches[chip_nr]->insert_idx >= BATCH_SIZE) {
            while (not m_hit_queue.push(hit_batches[chip_nr])) {
            }
            hit_batches[chip_nr] = new_batch(hit_batches[chip_nr], chip_nr);
        }
    }
    void insert_trigger(uint64_t data) {
        trigger_batch->data[trigger_batch->insert_idx++] = data;
        if (trigger_batch->insert_idx >= BATCH_SIZE) {
            while (not m_trigger_queue.push(trigger_batch)) {
            }
            trigger_batch = new_batch(trigger_batch, 5);
        }
    }

    BatchHandle() {
        for (auto &batch : hit_batches) {
            batch = batch_pool.acquire_batch_blocking();
        }
        trigger_batch = batch_pool.acquire_batch_blocking();
    }
};

class Parser {
    BatchHandle m_batch_handle;
    size_t m_sequence_number{0};

   public:
    auto get_batch_pool() -> BatchHandle & { return m_batch_handle; }

    void parse_header(tpx3_helper::Header t_header) {
        for (const auto *data_ptr = t_header.start; data_ptr < t_header.end;
             data_ptr = std::next(data_ptr)) {
            const uint64_t data_entry = *data_ptr;
            auto nibble = to_nibble(data_entry);

            switch (nibble) {
                case HIT_NIBBLE: {
                    m_batch_handle.insert_hit(t_header.chip_nr, data_entry);
                    break;
                }
                case TRIGGER_NIBBLE: {
                    m_batch_handle.insert_trigger(data_entry);
                    break;
                }
                case FRAME_NIBBLE: {
                }
                default:
                    break;
            }
        }
    };

    int parse(MemoryMap &t_mmap) {
        constexpr auto num_runs = 50;
        auto now = std::chrono::high_resolution_clock::now();

        for (size_t run = 0; run < num_runs; run++) {
            const auto *running = t_mmap.begin();
            while (std::next(running) < t_mmap.end()) {
                auto opt = tpx3_helper::parse_header(running);
                if (not opt) {
                    return -1;
                }
                auto header = opt.value();
                running = header.end;
                if (header.end > t_mmap.end()) {
                    return -1;
                }
                parse_header(header);
            }
            std::cout << "Run " << run + 1 << " completed\n";
        }

        auto end = std::chrono::high_resolution_clock::now();
        std::cout << "Throughput: "
                  << std::distance(t_mmap.begin(), t_mmap.end()) / 1024.0 /
                         1024.0 / 1024.0 * 8 /
                         std::chrono::duration<double>(end - now).count() *
                         num_runs
                  << " GB/s" << std::endl;
        return 0;
    };
};

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
                for (size_t i = 0; i < batch->insert_idx; ++i) {
                    auto data = batch->data[i];
                    auto hit = tpx3_helper::Hit(data);
                    asm volatile("" ::"g"(hit) : "memory");
                }

                batch->insert_idx = 0;
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
        es::lockfree::mpmc_queue<Batch<uint64_t> *, BATCH_POOL_SIZE> &queue,
        Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool,
        std::stop_token stop) {
        Batch<uint64_t> *batch;
        while (not stop.stop_requested()) {
            if (queue.pop(batch)) {
                for (size_t i = 0; i < batch->insert_idx; ++i) {
                    auto data = batch->data[i];
                    auto trigger = tpx3_helper::Trigger(data);
                    asm volatile("" ::"g"(trigger) : "memory");
                }

                batch->insert_idx = 0;
                batch_pool.release_batch_blocking(batch);
            }
        }
    }
    void start(
        es::lockfree::mpmc_queue<Batch<uint64_t> *, BATCH_POOL_SIZE> &queue,
        Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool) {
        m_thread =
            std::jthread([this, &queue, &batch_pool](std::stop_token stop) {
                (*this)(queue, batch_pool, stop);
            });
    }
    void stop() { m_thread.request_stop(); }
};

int main() {
    auto mmap = MemoryMap("./tpx3.tpx3");
    Parser parser;

    std::array<Hit_Consumer, 2> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(parser.get_batch_pool().m_hit_queue,
                               parser.get_batch_pool().batch_pool);
    }
    std::array<Trigger_Consumer, 2> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(parser.get_batch_pool().m_trigger_queue,
                                   parser.get_batch_pool().batch_pool);
    }

    parser.parse(mmap);

    for (auto &consumer : hit_consumers) {
        consumer.stop();
    }
    for (auto &consumer : trigger_consumers) {
        consumer.stop();
    }

    return 0;
}
