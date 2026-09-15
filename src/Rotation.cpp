//
// Created by jholder on 29/05/24.
//
#include "Rotation.hpp"
#include <stdexcept>
Rotation Rotation::from_string(const std::string &t_orientation) {
    if (t_orientation == "LtRBtT") {
        // x, 255 - y
        return {0, 1, 0, 1, 0, -1, t_orientation};
    }
    if (t_orientation == "TtBLtR") {
        // y, x
        return {0, 0, 1, 0, 1, 0, t_orientation};
    }
    if (t_orientation == "RtLTtB") {
        // 255 - x, y
        return {1, -1, 0, 0, 0, 1, t_orientation};
    }
    if (t_orientation == "BtTLtR") {
        // y, 255 - x
        return {0, 0, 1, 1, -1, 0, t_orientation};
    }
    if (t_orientation == "RtLBtT") {
        // 255 - x, 255 - y
        return {1, -1, 0, 1, 0, -1, t_orientation};
    }
    if (t_orientation == "TtBRtL") {
        // 255 - y, x
        return {1, 0, -1, 0, 1, 0, t_orientation};
    }
    if (t_orientation == "LtRTtB") {
        // x, y
        return {0, 1, 0, 0, 0, 1, t_orientation};
    }
    if (t_orientation == "BtTRtL") {
        // 255 - y, 255 - x
        return {1, 0, -1, 1, -1, 0, t_orientation};
    }
    throw std::runtime_error("Unknown rotation string: " + t_orientation);
}
