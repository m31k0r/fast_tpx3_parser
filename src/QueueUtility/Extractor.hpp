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
    using VariantPtr = std::variant<OwnedBatchPtr, BatchPtr, EndOfFile>;

    using IteratorType = std::conditional_t<
        std::is_same_v<BatchPtr, HitBatchPtr>, HitIterator,
        std::conditional_t<std::is_same_v<BatchPtr, TriggerBatchPtr>,
                           TriggerIterator, U64Iterator>>;

    InQueue &queue;
    size_t read_idx = 0;

    VariantPtr current_batch_obj;
    Batch *current_batch;

    std::deque<QueueMessage<OwnedBatchPtr>> overflow;
    bool ended = false;
    bool eof_queued = false;

    void copy_front() {
        auto *queued = queue.front();
        if constexpr (SCQueueable<InQueue, QueueMessage<BatchPtr>>) {
            if (std::holds_alternative<EndOfFile>(*queued)) {
                overflow.emplace_back(EndOfFile{});
                eof_queued = true;
            } else {
                overflow.emplace_back(
                    std::make_unique<Batch>(*std::get<BatchPtr>(*queued)));
            }
        } else {
            overflow.emplace_back(std::make_unique<Batch>(**queued));
        }
        queue.pop();
    }

    void load_next_batch() {
        if (overflow.empty() && not ended && queue.front()) {
            QueueMessage<BatchPtr> message = std::move(*queue.front());
            queue.pop();
            if (std::holds_alternative<EndOfFile>(message)) {
                ended = true;
                current_batch = nullptr;
                current_batch_obj = EndOfFile{};
                return;
            }
            read_idx = 0;
            current_batch = std::get<BatchPtr>(message).get();
            current_batch_obj = std::move(std::get<BatchPtr>(message));
            return;
        }
        // if (overflow.empty() && not ended && queue.front()) copy_front();
        if (not overflow.empty()) {
            QueueMessage<OwnedBatchPtr> message = std::move(overflow.front());
            overflow.pop_front();
            if (std::holds_alternative<EndOfFile>(message)) {
                ended = true;
                current_batch_obj = EndOfFile{};
                current_batch = nullptr;
                return;
            }
            read_idx = 0;
            current_batch = std::get<OwnedBatchPtr>(message).get();
            current_batch_obj = std::move(std::get<OwnedBatchPtr>(message));
            return;
        }
        current_batch = nullptr;
        current_batch_obj = OwnedBatchPtr(nullptr);
        return;
    }

   public:
    Extractor(InQueue &queue) : queue(queue) {}
    bool eof() const { return ended; }

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
    // Copy queued batches into an unbounded local buffer, returning the
    // originals to the pool so the producer can continue.
    void drain() {
        while (not eof_queued && not ended && queue.front()) {
            copy_front();
        }
    }
    size_t size() { return queue.size() + overflow.size(); }
};
