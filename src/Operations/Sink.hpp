#include <stop_token>
#include <thread>
#include "../Batch/Batch.hpp"
#include "../QueueUtility/QueueConcepts.hpp"

template <typename Queue, typename BatchPtr>
class Sink {
    std::jthread m_thread;

   public:
    std::stop_token get_stop_token() { return m_thread.get_stop_token(); }
    void operator()(Queue &queue)
        requires SCQueueable<Queue, BatchPtr>||MCQueueable<Queue, BatchPtr> {
            if constexpr (MCQueueable<Queue, BatchPtr>) {
                BatchPtr batch;
                queue.try_pop(batch);
            } else {
                if (queue.front()) {
                    queue.pop();
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
};