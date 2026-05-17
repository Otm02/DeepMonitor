#ifndef WINDOW_H
#define WINDOW_H

#include "config.h"

namespace window {
    struct Entry {
        float mean_vib;
        float mean_temp;
    };

    struct WindowState {
        Entry buffer[config::WINDOW_SIZE];
        int head;
        int count;
    };

    void append(WindowState& state, float mean_vib, float mean_temp);
    void to_flat_array(const WindowState& state, float out[config::INPUT_DIM]);
}

#endif // WINDOW_H
