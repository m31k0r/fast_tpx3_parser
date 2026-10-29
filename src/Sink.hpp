#include <stop_token>
#include <thread>
#include "Batch.hpp"
#include "QueueConcepts.hpp"

template <typename Queue>
class Sink {
    std::jthread m_thread;

   public:
    std::stop_token get_stop_token() { return m_thread.get_stop_token(); }
    void operator()(Queue &queue)
        requires SCQueueable<Queue, U64BatchPtr>;

    void operator()(Queue &queue)
        requires MCQueueable<Queue, U64BatchPtr>;

    void operator()(Queue &queue)
        requires SCQueueable<Queue, TriggerBatchPtr>;

    void operator()(Queue &queue)
        requires MCQueueable<Queue, TriggerBatchPtr>;
    void operator()(Queue &queue)
        requires SCQueueable<Queue, HitBatchPtr>;

    void operator()(Queue &queue)
        requires MCQueueable<Queue, HitBatchPtr>;

    void start(Queue &queue) {
        m_thread = std::jthread([this, &queue](std::stop_token stop) {
            pthread_setname_np(pthread_self(), "Sink");
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
template <typename Queue>
void Sink<Queue>::operator()(Queue &queue)
    requires MCQueueable<Queue, TriggerBatchPtr>
{
    U64BatchPtr ba;
    queue.try_pop(ba);
}

template <typename Queue>
void Sink<Queue>::operator()(Queue &queue)
    requires SCQueueable<Queue, TriggerBatchPtr>
{
    if (queue.front()) queue.pop();
}
template <typename Queue>
void Sink<Queue>::operator()(Queue &queue)
    requires MCQueueable<Queue, HitBatchPtr>
{
    U64BatchPtr ba;
    queue.try_pop(ba);
}

template <typename Queue>
void Sink<Queue>::operator()(Queue &queue)
    requires SCQueueable<Queue, HitBatchPtr>
{
    if (queue.front()) queue.pop();
}
