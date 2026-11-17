#pragma once
#include <assert.hpp>
#include <deque>
#include <memory>
#include <type_traits>

#include "../Batch/Batch.hpp"
#include "QueueConcepts.hpp"
#include "QueueMessage.hpp"

#include "Iterator.hpp"

template <typename InQueue, typename BatchPtr>
    requires(SCQueueable<InQueue, BatchPtr> ||
             SCQueueable<InQueue, QueueMessage<BatchPtr>>)

class Extractor {
    using Batch = typename BatchPtr::element_type;
    using OwnedBatchPtr = std::unique_ptr<Batch>;
    using VariantPtr = std::variant<OwnedBatchPtr, BatchPtr, EndOfStream, NewFile>;

    using IteratorType = std::conditional_t<
        std::is_same_v<BatchPtr, HitBatchPtr>, HitIterator,
        std::conditional_t<std::is_same_v<BatchPtr, TriggerBatchPtr>,
                           TriggerIterator, U64Iterator>>;

    InQueue &queue;
    size_t read_idx = 0;

    VariantPtr current_batch_obj;
    Batch *current_batch = nullptr;

    std::deque<QueueMessage<OwnedBatchPtr>> overflow;
    bool control_queued = false;
    bool eof_reached = false;
    bool new_file_reached = false;

    void load_message(QueueMessage<OwnedBatchPtr> message) {
        if (std::holds_alternative<EndOfStream>(message)) {
            eof_reached = true;
            current_batch = nullptr;
            current_batch_obj = EndOfStream{};
        } else if (std::holds_alternative<NewFile>(message)) {
            new_file_reached = true;
            current_batch = nullptr;
            current_batch_obj = std::get<NewFile>(std::move(message));
        } else {
            read_idx = 0;
            current_batch = std::get<OwnedBatchPtr>(message).get();
            current_batch_obj = std::move(std::get<OwnedBatchPtr>(message));
        }
    }

    void copy_front() {
        if constexpr (SCQueueable<InQueue, QueueMessage<BatchPtr>>) {
            auto *queued = queue.front();
            if (std::holds_alternative<EndOfStream>(*queued)) {
                overflow.emplace_back(EndOfStream{});
                control_queued = true;
            } else if (std::holds_alternative<NewFile>(*queued)) {
                overflow.emplace_back(std::get<NewFile>(*queued));
                control_queued = true;
            } else {
                overflow.emplace_back(std::make_unique<Batch>(
                    *std::get<BatchPtr>(*queued)));
            }
        } else {
            overflow.emplace_back(
                std::make_unique<Batch>(*(*queue.front())));
        }
        queue.pop();
    }

    void load_next_batch() {
        current_batch = nullptr;
        if (eof_reached || new_file_reached) return;
        if (overflow.empty() && queue.front()) {
            if constexpr (SCQueueable<InQueue, QueueMessage<BatchPtr>>) {
                QueueMessage<BatchPtr> message = std::move(*queue.front());
                queue.pop();
                if (std::holds_alternative<EndOfStream>(message)) {
                    load_message(EndOfStream{});
                    return;
                }
                if (std::holds_alternative<NewFile>(message)) {
                    load_message(std::get<NewFile>(std::move(message)));
                    return;
                }
                read_idx = 0;
                current_batch = std::get<BatchPtr>(message).get();
                current_batch_obj = std::move(std::get<BatchPtr>(message));
            } else {
                read_idx = 0;
                current_batch = queue.front()->get();
                current_batch_obj = std::move(*queue.front());
                queue.pop();
            }
            return;
        }
        if (not overflow.empty()) {
            QueueMessage<OwnedBatchPtr> message = std::move(overflow.front());
            overflow.pop_front();
            if (std::holds_alternative<EndOfStream>(message) ||
                std::holds_alternative<NewFile>(message)) {
                control_queued = false;
            }
            load_message(std::move(message));
            return;
        }
        current_batch = nullptr;
        current_batch_obj = OwnedBatchPtr(nullptr);
    }

   public:
    Extractor(InQueue &queue) : queue(queue) {}

    IteratorType front() {
        while (not current_batch || read_idx >= current_batch->size) {
            load_next_batch();
            if (not current_batch) {
                return IteratorType(nullptr, 0);
            }
        }
        return IteratorType(current_batch, read_idx);
    };

    void pop() {
        DEBUG_ASSERT(current_batch);
        DEBUG_ASSERT(read_idx < current_batch->size);
        read_idx++;
    };

    void drain() {
        while (queue.front() && not control_queued && not eof_reached &&
               not new_file_reached) {
            copy_front();
        }
    }

    bool eof() const { return eof_reached; }
    bool new_file() const { return new_file_reached; }
    const NewFile &file_message() const {
        DEBUG_ASSERT(new_file_reached);
        return std::get<NewFile>(current_batch_obj);
    }

    void consume_new_file() {
        DEBUG_ASSERT(new_file_reached);
        new_file_reached = false;
        current_batch_obj = OwnedBatchPtr(nullptr);
    }

    size_t size() { return queue.size() + overflow.size(); }
};
