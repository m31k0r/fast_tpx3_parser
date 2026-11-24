#pragma once

#include <variant>

struct EndOfStream {};
struct NewFile{std::string filename;};

template <typename BatchPtr>
using QueueMessage = std::variant<BatchPtr, EndOfStream, NewFile>;
