#include <memory>
#include <thread>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Constants.hpp"
#include "Merging_Buffer.hpp"
#include "QueueConcepts.hpp"
#include "tpx3_helper.hpp"

template <typename Queue>
class Sink {
    std::jthread m_thread;

   public:
    void operator()(Queue &queue)
        requires SCQueueable<Queue, U64BatchPtr>;

    void operator()(Queue &queue)
        requires MCQueueable<Queue, U64BatchPtr>;
    void start(Queue &queue) {
        m_thread = std::jthread([this, &queue](std::stop_token stop) {
            while (not stop.stop_requested()) {
                (*this)(queue);
            }
        });
    }
};
template <typename Queue>
void Sink<Queue>::operator()(Queue &queue)
    requires MCQueueable<Queue, U64BatchPtr>
{
    U64BatchPtr ba;
    queue.try_pop(ba);
}

template <typename Queue>
void Sink<Queue>::operator()(Queue &queue)
    requires SCQueueable<Queue, U64BatchPtr>
{
    if (queue.front()) queue.pop();
}
