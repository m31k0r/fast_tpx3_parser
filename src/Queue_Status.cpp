#include "Queue_Status.hpp"

#include <print>
#include <ranges>

void print_queue_status(const QueueVariant& queue,
                        std::string_view queue_name) {
    std::visit(
        [&](const auto* q) {
            if (q) {
                std::println("Queue {} has {} elements", queue_name, q->size());
            }
        },
        queue);
}

void Queue_State_Printer::run() {
    while (!m_thread.get_stop_token().stop_requested()) {
        std::println("--------------------------------------------------");

        for (const auto& [queue, name] :
             std::views::zip(m_queues, m_queue_names)) {
            print_queue_status(queue, name);
        }

        std::println("--------------------------------------------------");

        std::this_thread::sleep_for(m_interval);
    }
}

Queue_State_Printer::Queue_State_Printer(std::vector<QueueVariant> queues,
                                         std::vector<std::string> queue_names,
                                         std::chrono::milliseconds interval)
    : m_queues(std::move(queues)),
      m_queue_names(std::move(queue_names)),
      m_interval(interval),
      m_thread(&Queue_State_Printer::run, this) {}
