#pragma once

#include <cstdint>
#include <iostream>
#include <stop_token>
#include <thread>
#include "Batch/Batch.hpp"
#include "Batch/Batch_Pool.hpp"
#include "Constants.hpp"
#include "Parsing/Camera.hpp"
#include "QueueUtility/Extractor.hpp"
#include "QueueUtility/Injector.hpp"
#include "Utility/AdaptivBackoff.hpp"

template <typename HitQueue, typename TriggerQueue, typename OutQueue>
class Delay {
    enum class Step { Continue, Finished, Stopped };

    Extractor<HitQueue, HitBatchPtr> hit_extractor;
    Extractor<TriggerQueue, TriggerBatchPtr> trigger_extractor;
    Injector<OutQueue, HitBatchPtr, Batch_Pool<HitBatch>, Hit> inj;
    Camera& camera;
    uint64_t last_delay_time = 0;
    std::jthread m_thread;

    // No hit available: drop a trailing trigger or finish once both ended.
    Step handle_missing_hit(const auto& first_trigger, std::stop_token stop) {
        trigger_extractor.drain();
        if (not hit_extractor.eof()) return Step::Continue;
        if (first_trigger) {
            trigger_extractor.pop();
        } else if (trigger_extractor.eof()) {
            inj.finish(stop);
            return Step::Finished;
        }
        return Step::Continue;
    }

    // Hit lies between the previous trigger (last_delay_time) and the next
    // one; trigger_diff is the interval between them, 0 if there is no next.
    Step emit_hit(const auto& hit, const auto& next_trigger,
                  std::stop_token stop) {
        adaptive_backoff backoff;
        const uint64_t trigger_diff =
            next_trigger ? next_trigger.toa() - last_delay_time : 0;
        const Hit out{hit.toa(),
                      hit.tot(),
                      hit.x(),
                      hit.y(),
                      static_cast<uint32_t>(hit.toa() - last_delay_time),
                      static_cast<uint32_t>(trigger_diff)};
        while (not inj.try_push(out)) {
            backoff.pause();
            if (stop.stop_requested()) return Step::Stopped;
        }
        hit_extractor.pop();
        return Step::Continue;
    }

    Step step(std::stop_token stop) {
        // hit_extractor.drain();
        // trigger_extractor.drain();
        const auto first_trigger = trigger_extractor.front();
        const auto first_hit = hit_extractor.front();

        if (not first_hit) return handle_missing_hit(first_trigger, stop);
        // Wait until the next trigger is known or triggers have ended.
        if (not first_trigger && not trigger_extractor.eof())
            return Step::Continue;

        if (first_trigger) {
            if (first_trigger.type() != camera.delay_trigger_type) {
                trigger_extractor.pop();
                std::cout << "TD";
                return Step::Continue;
            }
            if (first_hit.toa() > first_trigger.toa()) {
                last_delay_time = first_trigger.toa();
                trigger_extractor.pop();
                return Step::Continue;
            }
        }
        const auto result = emit_hit(first_hit, first_trigger, stop);
        return result;
    }

    void run(std::stop_token stop) {
        pthread_setname_np(pthread_self(), "Delay");
        while (not stop.stop_requested()) {
            if (step(stop) != Step::Continue) return;
        }
        std::cout << "Hits left: " << hit_extractor.size() << std::endl;
        std::cout << "Trigger left: " << trigger_extractor.size() << std::endl;
    }

   public:
    Delay(HitQueue& hit, TriggerQueue& trigger, OutQueue& out,
          Batch_Pool<HitBatch>& pool, Camera& camera)
        : hit_extractor(hit),
          trigger_extractor(trigger),
          inj(out, pool),
          camera(camera) {
        m_thread = std::jthread([this](std::stop_token stop) { run(stop); });
    }
};
