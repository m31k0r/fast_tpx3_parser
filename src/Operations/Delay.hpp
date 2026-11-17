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

template<typename HitQueue, typename TriggerQueue, typename OutQueue>
class Delay {
    enum class Step { Continue, Finished, Stopped };

    Extractor<HitQueue, HitBatchPtr> hit_extractor;
    Extractor<TriggerQueue, TriggerBatchPtr> trigger_extractor;
    Injector<OutQueue, HitBatchPtr, Batch_Pool<HitBatch>, Hit> inj;
    Camera &camera;
    uint64_t last_delay_time = 0;
    uint64_t diff_time = 0;
    std::jthread m_thread;

    bool at_boundary(const auto& extractor) const {
        return extractor.eof() || extractor.new_file();
    }

    Step finish_boundary(std::stop_token stop) {
        ASSERT(hit_extractor.eof() == trigger_extractor.eof(),
               "Mismatched hit and trigger stream boundaries");
        if (hit_extractor.eof() && trigger_extractor.eof()) {
            inj.finish(stop);
            return Step::Finished;
        }
        if (hit_extractor.new_file() && trigger_extractor.new_file()) {
            ASSERT(hit_extractor.file_message().filename ==
                       trigger_extractor.file_message().filename,
                   "Mismatched hit and trigger files");
            inj.new_file(hit_extractor.file_message().filename, stop);
            hit_extractor.consume_new_file();
            trigger_extractor.consume_new_file();
            last_delay_time = 0;
            diff_time = 0;
        }
        return Step::Continue;
    }

    // No hit available: drop trailing triggers or finish at a shared boundary.
    Step handle_missing_hit(const auto &first_trigger, std::stop_token stop) {
        trigger_extractor.drain();
        if (not at_boundary(hit_extractor)) return Step::Continue;
        if (first_trigger) {
            trigger_extractor.pop();
        } else if (at_boundary(trigger_extractor)) {
            return finish_boundary(stop);
        }
        return Step::Continue;
    }

    // Hit lies between the previous trigger (last_delay_time) and the next
    // one; trigger_diff is the interval between them, 0 if there is no next.
    Step emit_hit(const auto &hit,
                  std::stop_token stop) {
        adaptive_backoff backoff;
        const Hit out{
            hit.toa(),
            hit.tot(),
            hit.x(),
            hit.y(),
            static_cast<uint32_t>(last_delay_time==0 ? 0:hit.toa() - last_delay_time),
            static_cast<uint32_t>(diff_time)
        };
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
        if (not first_trigger && not at_boundary(trigger_extractor)) {
            hit_extractor.drain();
            return Step::Continue;
        }

        if (not at_boundary(trigger_extractor)) {
            DEBUG_ASSERT(first_trigger);
            if (first_trigger.type() == camera.diff_trigger_type) {
                DEBUG_ASSERT(diff_time==0);
                diff_time = first_trigger.toa() - last_delay_time;
                trigger_extractor.pop();
                return Step::Continue;
            }
            if (first_trigger.type() != camera.delay_trigger_type) {
                trigger_extractor.pop();
                return Step::Continue;
            }
            if (first_hit.toa() > first_trigger.toa()) {
                last_delay_time = first_trigger.toa();
                diff_time = 0;
                trigger_extractor.pop();
                return Step::Continue;
            }
        }
        std::cout<<std::flush;
            const auto result = emit_hit(first_hit, stop);
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
    Delay(HitQueue &hit, TriggerQueue &trigger, OutQueue &out,
          Batch_Pool<HitBatch> &pool, Camera &camera)
        : hit_extractor(hit),
          trigger_extractor(trigger),
          inj(out, pool),
          camera(camera) {
        m_thread = std::jthread([this](std::stop_token stop) { run(stop); });
    }
};
