#pragma once

#include <cstdint>
#include <iostream>
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
            pthread_setname_np(pthread_self(), "Delay");
            while (not stop.stop_requested()) {
                hit_extractor.drain();
                trigger_extractor.drain();
                const auto first_trigger = trigger_extractor.front();
                const auto first_hit = hit_extractor.front();
                if (not first_hit) {
                    if (hit_extractor.eof()) {
                        if (first_trigger) trigger_extractor.pop();
                        else if (trigger_extractor.eof()) {
                            inj.finish(stop);
                            return;
                        }
                    }
                    continue;
                }
                if (not first_trigger && not trigger_extractor.eof()) continue;
                if (first_trigger && first_trigger.type() != 0xf) {
                    trigger_extractor.pop();
                    std::cout << "TD";
                    continue;
                }
                if (first_trigger && first_hit.toa() > first_trigger.toa()) {
                    last_delay_time = first_trigger.toa();
                    trigger_extractor.pop();
                    continue;
                } else {
                    adaptive_backoff backoff;
                    while (not inj.try_push(
                        {first_hit.toa(), first_hit.tot(), first_hit.x(),
                         first_hit.y(),
                         static_cast<uint16_t>(first_hit.toa() -
                                               last_delay_time)})) {
                        backoff.pause();
                        if (stop.stop_requested()) return;
                    }
                    hit_extractor.pop();
                }
                std::cout << std::flush;
            }

            std::cout << "Hits left: " << hit_extractor.size() << std::endl;
            std::cout << "Trigger left: " << trigger_extractor.size()
                      << std::endl;
        });
    }
};
