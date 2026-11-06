// #include "Operations/Sorter.hpp"
// #include <catch2/catch_test_macros.hpp>
// #include <chrono>
// #include <thread>
// #include <vector>
// #include "Batch/Batch.hpp"
// #include "Batch/Batch_Pool.hpp"
// #include "QueueUtility/SPSCQueue.hpp"
// #include "Utility/SPSCQueue.h"
// namespace {
// using InputQueue = rigtorp::SPSCQueue<U64BatchPtr>;
// using OutputQueue = rigtorp::SPSCQueue<HitBatchPtr>;
// uint64_t make_hit_data(size_t timestamp) {
//     const auto spidr = timestamp / (1U << 14);
//     const auto toa = timestamp % (1U << 14);
//     return (static_cast<uint64_t>(toa) << 30) | spidr;
// }
// void push_batch(InputQueue &queue, Batch_Pool<U64Batch> &pool,
//                 size_t first_timestamp, size_t count) {
//     auto batch = pool.acquire_batch_blocking();
//     for (size_t i = 0; i < count; ++i) {
//         batch->push_back(make_hit_data(first_timestamp + count - i - 1));
//     }
//     queue.push(std::move(batch));
// }
// std::vector<uint64_t> collect_batches(OutputQueue &queue, size_t count) {
//     std::vector<uint64_t> times;
//     const auto deadline =
//         std::chrono::steady_clock::now() + std::chrono::seconds(5);
//     while (count != 0 && std::chrono::steady_clock::now() < deadline) {
//         if (auto *batch = queue.front()) {
//             REQUIRE(*batch);
//             times.insert(times.end(), (*batch)->toa.begin(),
//                          (*batch)->toa.begin() + (*batch)->size);
//             queue.pop();
//             --count;
//         } else {
//             std::this_thread::yield();
//         }
//     }
//     REQUIRE(count == 0);
//     return times;
// }
// }  // namespace
// TEST_CASE("Sorter emits and sorts an initial partial batch", "[Sorter]") {
//     Batch_Pool<U64Batch> input_pool(2);
//     Batch_Pool<HitBatch> output_pool(2);
//     InputQueue input(2);
//     OutputQueue output(2);
//     Camera camera;
//     Sort<InputQueue, OutputQueue> sorter(input, output, output_pool, camera);
//     sorter.start_worker();
//     push_batch(input, input_pool, 0, 5);
//     const auto times = collect_batches(output, 1);
//     REQUIRE(times.size() == 5);
//     REQUIRE(std::ranges::is_sorted(times));
// }
// TEST_CASE("Sorter emits the retained hits and trailing partial batch",
//           "[Sorter]") {
//     Batch_Pool<U64Batch> input_pool(4);
//     Batch_Pool<HitBatch> output_pool(4);
//     InputQueue input(4);
//     OutputQueue output(4);
//     Camera camera;
//     Sort<InputQueue, OutputQueue> sorter(input, output, output_pool, camera);
//     sorter.start_worker();
//     push_batch(input, input_pool, 0, BATCH_SIZE);
//     push_batch(input, input_pool, BATCH_SIZE, BATCH_SIZE);
//     push_batch(input, input_pool, 2 * BATCH_SIZE, 5);
//     const auto times = collect_batches(output, 3);
//     REQUIRE(times.size() == 2 * BATCH_SIZE + 5);
//     REQUIRE(std::ranges::is_sorted(times));
// }
