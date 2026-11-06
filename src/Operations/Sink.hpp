#include <stop_token>
#include <thread>
#include "../Batch/Batch.hpp"
#include "../QueueUtility/QueueConcepts.hpp"

template <typename Queue, typename BatchPtr>
class Sink {
    std::jthread m_thread;
    size_t counter_batch = 0;
    size_t counter = 0;

    uint64_t previous_time = 0;

   public:
    std::stop_token get_stop_token() { return m_thread.get_stop_token(); }
    void operator()(Queue &queue)
        requires SCQueueable<Queue, BatchPtr> || MCQueueable<Queue, BatchPtr>
    {
        if constexpr (MCQueueable<Queue, BatchPtr>) {
            BatchPtr batch;
            if (queue.try_pop(batch)) {
                counter_batch++;
                for (size_t i = 0; i < batch->size; ++i) {
                    // DEBUG_ASSERT(previous_time <= batch->toa[i],
                    //              std::format("i={}", i));
                    // previous_time = batch->toa[i];
                    counter++;
                }
            }
        } else {
            if (queue.front()) {
                auto batch = std::move(*queue.front());
                for (size_t i = 0; i < batch->size; ++i) {
                    // DEBUG_ASSERT(previous_time <= batch->toa[i],
                    //              std::format("i={}", i));
                    // previous_time = batch->toa[i];
                    counter++;
                }
                queue.pop();
                counter_batch++;
            }
        }
    }

    void start(Queue &queue) {
        m_thread = std::jthread([this, &queue](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sink");
            while (not stop.stop_requested()) {
                (*this)(queue);
            }
        });
    }
    ~Sink() {
        std::cout << "Received " << counter_batch << " batches with " << counter
                  << " events\n";
    }
};
