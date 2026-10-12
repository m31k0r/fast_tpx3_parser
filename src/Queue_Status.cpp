#include "Queue_Status.hpp"
#include <print>
#include <ranges>
#include "Batch.hpp"
#include "Constants.hpp"
#include "MPMCQueue.h"

void print_queue_status(const rigtorp::MPMCQueue<U64BatchPtr> *queue,
                        std::string_view queue_name) {
    std::println("Queue {} has {}/{} elements", queue_name, queue->size(),
                 QUEUE_SIZE);
}
void Queue_State_Printer::run() {
    while (not m_thread.get_stop_token().stop_requested()) {
        std::println("--------------------------------------------------");
        for (const auto &[queue, name] :
             std::ranges::views::zip(m_queues, m_queue_names)) {
            print_queue_status(queue, name);
        }
        std::println("--------------------------------------------------");
        std::this_thread::sleep_for(m_interval);
    }
}
Queue_State_Printer::Queue_State_Printer(
    const std::vector<rigtorp::MPMCQueue<U64BatchPtr> *> &queues,
    std::vector<std::string> queue_names, std::chrono::milliseconds interval)
    : m_queues(queues),
      m_thread(&Queue_State_Printer::run, this),
      m_interval(interval),
      m_queue_names(queue_names) {}
