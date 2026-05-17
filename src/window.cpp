#include "window.h"

#include <algorithm> // std::min

namespace window {

void append(WindowState& state, float mean_vib, float mean_temp) {
    state.buffer[state.head].mean_vib = mean_vib;
    state.buffer[state.head].mean_temp = mean_temp;
    state.head = (state.head + 1) % config::WINDOW_SIZE;
    state.count = std::min(state.count + 1, config::WINDOW_SIZE);
}

void to_flat_array(const WindowState& state, float out[config::INPUT_DIM]) {
    // Zero the entire output first
    for (int i = 0; i < config::INPUT_DIM; i++) {
        out[i] = 0.0f;
    }

    // Determine the start index for temporal ordering (oldest entry first)
    int start;
    if (state.count < config::WINDOW_SIZE) {
        start = 0; // Partial buffer: entries start at index 0
    } else {
        start = state.head; // Full buffer: head points to the oldest entry
    }

    for (int i = 0; i < state.count; i++) {
        int buf_idx = (start + i) % config::WINDOW_SIZE;
        int out_idx = i * 2;
        out[out_idx]     = state.buffer[buf_idx].mean_vib;
        out[out_idx + 1] = state.buffer[buf_idx].mean_temp;
    }
}

} // namespace window
