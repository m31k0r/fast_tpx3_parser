#pragma once
#include <assert.hpp>
#include "Batch.hpp"
#include "QueueConcepts.hpp"
template <typename InQueue, typename OutQueue>
    requires SCQueueable<InQueue, uint64_t> && InQueueable<OutQueue, uint64_t>
class Delay {
    InQueue& trigger_queue;
    InQueue& hit_queue;

    uint64_t current_hit = 0;
    uint64_t current_trigger = 0;

    OutQueue& out_queue;
    bool is_ready() {
        if (current_hit and current_trigger) return true;
        return false;
    }

   public:
    bool startup() {
        if (not current_hit) {
            if (hit_queue.front()) {
                current_hit = *(hit_queue.front());
            }
        }
        if (not current_trigger) {
            if (trigger_queue.front()) {
                current_trigger = *(trigger_queue.front());
            }
        }
        return is_ready();
    }
    void step() { DEBUG_ASSERT(is_ready()); }
    Delay(InQueue& hit_queue, InQueue& trigger_queue, OutQueue& out_queue)
        : trigger_queue(trigger_queue),
          hit_queue(hit_queue),
          out_queue(out_queue) {}
};
