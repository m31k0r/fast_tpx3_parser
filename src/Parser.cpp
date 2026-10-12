#include "Parser.hpp"
void Parser::parse_header(tpx3_helper::Header t_header) {
    for (const auto *data_ptr = t_header.start; data_ptr < t_header.end;
         data_ptr = std::next(data_ptr)) {
        const uint64_t data_entry = *data_ptr;
        auto nibble = to_nibble(data_entry);

        switch (nibble) {
            case HIT_NIBBLE: {
                m_batch_handle.insert_hit(t_header.chip_nr, data_entry);
                break;
            }
            case TRIGGER_NIBBLE: {
                m_batch_handle.insert_trigger(data_entry);
                break;
            }
            case FRAME_NIBBLE: {
            }
            default:
                break;
        }
    }
};
int Parser::parse(MemoryMap &t_mmap) {
    for (size_t run = 0; run < num_runs; run++) {
        const auto *running = t_mmap.begin();
        while (std::next(running) < t_mmap.end()) {
            auto opt = tpx3_helper::parse_header(running);
            if (not opt) {
                return -1;
            }
            auto header = opt.value();
            running = header.end;
            if (header.end > t_mmap.end()) {
                return -1;
            }
            parse_header(header);
        }
    }
    return 0;
};
