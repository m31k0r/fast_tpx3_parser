#include "Merging_Buffer.hpp"
#include <sys/types.h>
#include "Batch.hpp"
#include "Batch_Pool.hpp"
#include "Compare.hpp"
#include "Constants.hpp"
#include "Utility/MPMCQueue.h"

#include <catch2/catch_message.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <cstdint>
#include <print>

using Queue = rigtorp::SPSCQueue<U64BatchPtr>;

namespace {
TEST_CASE("MergingBuffer consume sequenced batches") {

}
}  // namespace
