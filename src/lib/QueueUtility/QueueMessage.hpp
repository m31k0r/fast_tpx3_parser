#pragma once

#include <string>
#include <variant>

struct EndOfStream {};
struct NewFile{std::string filename;};
struct Flush {};

template <typename BatchPtr>
using QueueMessage = std::variant<BatchPtr, EndOfStream, NewFile, Flush>;
