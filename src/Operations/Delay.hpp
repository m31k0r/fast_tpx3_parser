#pragma once

#include <cstdint>
#include <stop_token>
#include <thread>
#include "Batch/Batch.hpp"
#include "Batch/Batch_Pool.hpp"
#include "Constants.hpp"
#include "QueueUtility/Extractor.hpp"
#include "QueueUtility/Injector.hpp"
#include "Utility/AdaptivBackoff.hpp"

template <typename HitQueue, typename TriggerQueue, typename OutQueue>
class Delay {
    Extractor<HitQueue, HitBatchPtr> hit_extractor;
    Extractor<TriggerQueue, TriggerBatchPtr> trigger_extractor;
    Injector<OutQueue, HitBatchPtr, Batch_Pool<HitBatch>, Hit> inj;

    uint64_t last_delay_time = 0;
    std::jthread m_thread;

   public:
    Delay(HitQueue& hit, TriggerQueue& trigger, OutQueue& out,
          Batch_Pool<HitBatch>& pool)
        : hit_extractor(hit), trigger_extractor(trigger), inj(out, pool) {
        m_thread = std::jthread([this](std::stop_token stop) {
            while (not stop.stop_requested()) {
                if (hit_extractor.size() == 0) {
                    if (trigger_extractor.size() == QUEUE_SIZE) {
                        const auto first_trigger = trigger_extractor.front();
                        if (first_trigger.type() == 0xf)
                            last_delay_time = first_trigger.toa();
                        trigger_extractor.pop();
                    }
                    continue;
                }
                const auto first_trigger = trigger_extractor.front();
                const auto first_hit = hit_extractor.front();
                if (first_trigger.type() != 0xf) {
                    trigger_extractor.pop();
                    continue;
                }

                if (first_hit.toa() > first_trigger.toa()) {
                    last_delay_time = first_trigger.toa();
                    trigger_extractor.pop();
                    continue;
                } else {
                    adaptive_backoff backoff;
                    while (inj.try_push(
                        {first_hit.toa(), first_hit.tot(), first_hit.x(),
                         first_hit.y(),
                         static_cast<uint16_t>(first_hit.toa() -
                                               last_delay_time)})) {
                        backoff.pause();
                    }
                    std::cout << "H";
                    hit_extractor.pop();
                }
            }
        });
    }
};
