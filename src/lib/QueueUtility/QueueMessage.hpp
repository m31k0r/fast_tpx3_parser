#pragma once

#include <string>
#include <variant>

struct EndOfStream {};
struct NewFile{std::string filename;};
struct Flush {};

template <typename BatchPtr>
using QueueMessage = std::variant<BatchPtr, EndOfStream, NewFile, Flush>;

enum class QueueBoundary { None, EndOfStream, NewFile, Flush };

template <typename BatchPtr>
QueueBoundary queue_boundary(const QueueMessage<BatchPtr>& message) {
    if (std::holds_alternative<EndOfStream>(message))
        return QueueBoundary::EndOfStream;
    if (std::holds_alternative<NewFile>(message)) return QueueBoundary::NewFile;
    if (std::holds_alternative<Flush>(message)) return QueueBoundary::Flush;
    return QueueBoundary::None;
}
