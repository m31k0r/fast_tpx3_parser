#pragma once
#include <cstdint>
template<typename Batch>
struct Sequencer {
    uint64_t sequence_number = 0;

    void operator()(Batch &batch) {
        batch->sequence_number = sequence_number;
        sequence_number++;
    }
};

