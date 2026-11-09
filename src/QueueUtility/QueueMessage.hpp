#pragma once

#include <variant>

struct EndOfFile {};

template <typename BatchPtr>
using QueueMessage = std::variant<BatchPtr, EndOfFile>;
