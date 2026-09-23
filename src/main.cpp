#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <future>
#include <iostream>
#include <iterator>
#include <thread>

#include "Batch_Pool.hpp"
#include "Memory_Map.hpp"
#include "Sorting_Buffer.hpp"
#include "mpmc_queue.h"
#include "tpx3_helper.hpp"

constexpr size_t BATCH_SIZE = 500;
constexpr size_t BATCH_POOL_SIZE = 64 * 128;
constexpr size_t QUEUE_SIZE = 128;

constexpr auto num_runs = 50;

template <typename T>
struct Batch {
    std::array<T, BATCH_SIZE> data{};
    size_t insert_idx{0};
    std::atomic<bool> in_use{false};
};

template <typename Batch>
void exchange_batch_on_full(
    Batch *&batch, Batch_Pool<Batch, BATCH_POOL_SIZE> &batch_pool,
    es::lockfree::mpmc_queue<Batch *, QUEUE_SIZE> &queue) {
    if (batch->insert_idx >= BATCH_SIZE) {
        while (not queue.push(batch)) {
        }
        batch = batch_pool.acquire_batch_blocking();
    }
}

class BatchHandle {
   public:
    Batch<uint64_t> *hit_batches[4];
    Batch<uint64_t> *trigger_batch;
    Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> batch_pool;

    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> m_hit_queue[4];
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> m_trigger_queue;

    void insert_release(
        Batch<uint64_t> *&batch,
        es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &queue,
        uint64_t data) {
        exchange_batch_on_full(batch, batch_pool, queue);
        batch->data[batch->insert_idx++] = data;
    }

    void insert_hit(uint8_t chip_nr, uint64_t data) {
        insert_release(hit_batches[chip_nr], m_hit_queue[chip_nr], data);
    }
    void insert_trigger(uint64_t data) {
        insert_release(trigger_batch, m_trigger_queue, data);
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

   public:
    auto get_batch_handle() -> BatchHandle & { return m_batch_handle; }

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
        std::chrono::high_resolution_clock::time_point start;
        for (size_t run = 0; run < num_runs + 10; run++) {
            if (run == 10) {
                start = std::chrono::high_resolution_clock::now();
            }
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
                         std::chrono::duration<double>(end - start).count() *
                         num_runs
                  << " GB/s" << std::endl;
        return 0;
    };
};

template <typename Compare, size_t WINDOW_SIZE = 64>
class Sort {
    std::jthread m_thread;
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &m_hit_queue;
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &m_sorted_hit_queue;
    SortingBuffer<uint64_t, WINDOW_SIZE, Compare> m_buffer;

    Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &m_batch_pool;
    Batch<uint64_t> *m_current_batch;

   public:
    bool idle;

    Sort(es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> &hit_queue,
         es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
             &sorted_hit_queue,
         Batch_Pool<Batch<uint64_t>, BATCH_POOL_SIZE> &batch_pool)
        : m_hit_queue(hit_queue),
          m_sorted_hit_queue(sorted_hit_queue),
          m_batch_pool(batch_pool) {
        m_current_batch = m_batch_pool.acquire_batch_blocking();
    }
    void start_worker() {
        m_thread = std::jthread([this](std::stop_token stop) {
            Batch<uint64_t> *batch;
            while (not stop.stop_requested()) {
                if (m_hit_queue.pop(batch)) {
                    idle = false;
                    for (size_t i = 0; i < batch->insert_idx; ++i) {
                        auto data = batch->data[i];
                        auto sorted_data = m_buffer.pushpop(data);
                        if (sorted_data.has_value()) {
                            exchange_batch_on_full(m_current_batch,
                                                   m_batch_pool,
                                                   m_sorted_hit_queue);
                            m_current_batch
                                ->data[m_current_batch->insert_idx++] =
                                sorted_data.value();
                        }
                    }
                    m_batch_pool.release_batch_blocking(batch);
                } else {
                    idle = true;
                }
            }
        });
    }
    void stop() { m_thread.request_stop(); }
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
                for (size_t i = 0; i < batch->insert_idx; ++i) {
                    auto data = batch->data[i];
                    auto trigger = tpx3_helper::Trigger(data);
                    asm volatile("" ::"g"(trigger) : "memory");
                }
                //
                batch_pool.release_batch_blocking(batch);
            } else {
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

int main() {
    Parser parser;

    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE> sorted_hit_queue[4];
    es::lockfree::mpmc_queue<Batch<uint64_t> *, QUEUE_SIZE>
        sorted_trigger_queue;
    std::array<std::unique_ptr<Sort<Compare_Hits, 8>>, 8> sorters;
    for (int chip = 0; chip < 4; ++chip) {
        sorters[chip] = std::make_unique<Sort<Compare_Hits, 8>>(
            parser.get_batch_handle().m_hit_queue[chip], sorted_hit_queue[chip],
            parser.get_batch_handle().batch_pool);
        sorters[chip]->start_worker();
    }
    Sort<Compare_Triggers, 8> trigger_sorter(
        parser.get_batch_handle().m_trigger_queue, sorted_trigger_queue,
        parser.get_batch_handle().batch_pool);
    trigger_sorter.start_worker();

    std::array<Hit_Consumer, 4> hit_consumers;
    for (size_t i = 0; i < hit_consumers.size(); ++i) {
        hit_consumers[i].start(sorted_hit_queue[i],
                               parser.get_batch_handle().batch_pool);
    }
    std::array<Trigger_Consumer, 1> trigger_consumers;
    for (size_t i = 0; i < trigger_consumers.size(); ++i) {
        trigger_consumers[i].start(sorted_trigger_queue,
                                   parser.get_batch_handle().batch_pool);
    }

    std::future<void> future = std::async(std::launch::async, [&parser] {
        auto mmap = MemoryMap("./tpx3.tpx3");
        parser.parse(mmap);
    });

    while (not(future.wait_for(std::chrono::milliseconds(500)) ==
               std::future_status::ready)) {
    }

    for (auto &consumer : hit_consumers) {
        consumer.stop();
    }
    for (auto &consumer : trigger_consumers) {
        consumer.stop();
    }
    for (auto &sorter : sorters) {
        sorter->stop();
    }

    return 0;
}
