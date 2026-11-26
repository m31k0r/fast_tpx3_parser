#include <catch2/catch_test_macros.hpp>
#include <array>
#include <chrono>
#include <thread>
#include <variant>
#include "../Camera.hpp"
#include "Parser.hpp"

namespace {
class TestOutput {
    HitSPSC queue{8};

   public:
    bool try_push(QueueMessage<HitBatchPtr> message) {
        return queue.try_push(std::move(message));
    }
    QueueMessage<HitBatchPtr>* front() { return queue.front(); }
    void pop() { queue.pop(); }
};

auto wait_front(TestOutput& output) {
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::seconds(5);
    auto* message = output.front();
    while (!message && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
        message = output.front();
    }
    return message;
}
}  // namespace

TEST_CASE("Parser uses the supplied output and camera trigger types") {
    TestOutput output;
    Camera camera;
    Parser<1, TestOutput, Camera> parser(output, camera);
    const auto trigger = [](uint64_t type, uint64_t time) {
        return (TRIGGER_NIBBLE << 60) | (type << 56) | (time << 9) |
               (uint64_t{1} << 5);
    };
    const std::array<uint64_t, 4> words{
        trigger(camera.delay_trigger_type, 1),
        trigger(camera.diff_trigger_type, 2),
        (HIT_NIBBLE << 60) | (uint64_t{1} << 30),
        trigger(camera.delay_trigger_type, 10)};
    parser.parseHeader({words.data(), words.data() + words.size(), 0});
    parser.finish();

    auto* message = wait_front(output);
    REQUIRE(message);
    REQUIRE(std::holds_alternative<HitBatchPtr>(*message));
    auto& batch = std::get<HitBatchPtr>(*message);
    REQUIRE(batch->size == 1);
    CHECK(batch->toa[0] == 25000);
    CHECK(batch->delay[0] == 21875);
    CHECK(batch->trigger_diff[0] == 3125);
    output.pop();
    message = wait_front(output);
    REQUIRE(message);
    REQUIRE(std::holds_alternative<EndOfStream>(*message));
    output.pop();
}
