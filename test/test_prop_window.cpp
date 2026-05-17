#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "window.h"

/**
 * Property 5: Circular Buffer Maintains Recent Entries
 *
 * For any sequence of N appended entries (where N >= 0), the sliding window
 * should contain the most recent min(N, WINDOW_SIZE) entries in temporal
 * order, and the count should equal min(N, WINDOW_SIZE).
 *
 * **Validates: Requirements 3.1, 3.2, 3.4**
 */

// Generator for bounded floats in [-50, 50]
static rc::Gen<float> boundedFloat() {
    return rc::gen::map(rc::gen::inRange(-5000, 5001), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}

/**
 * **Validates: Requirements 3.1, 3.2, 3.4**
 *
 * Property: After appending N random entries to a fresh WindowState,
 * the count equals min(N, WINDOW_SIZE) and to_flat_array() contains
 * the most recent min(N, WINDOW_SIZE) entries in temporal order.
 * Format: [vib_0, temp_0, vib_1, temp_1, ...] where entry 0 is oldest.
 */
RC_GTEST_PROP(CircularBuffer, MaintainsRecentEntriesInOrder, ()) {
    // Generate a random number of entries to append (0 to 100)
    int N = *rc::gen::inRange(0, 101);

    // Generate N random (mean_vib, mean_temp) pairs
    std::vector<float> vibs(N);
    std::vector<float> temps(N);
    for (int i = 0; i < N; ++i) {
        vibs[i]  = *boundedFloat();
        temps[i] = *boundedFloat();
    }

    // Initialize WindowState to zeros
    window::WindowState state;
    std::memset(&state, 0, sizeof(state));

    // Append all N entries
    for (int i = 0; i < N; ++i) {
        window::append(state, vibs[i], temps[i]);
    }

    int expected_count = std::min(N, config::WINDOW_SIZE);

    // Verify count
    RC_ASSERT(state.count == expected_count);

    // Get the flat array
    float flat[config::INPUT_DIM];
    window::to_flat_array(state, flat);

    // The most recent expected_count entries should appear in temporal order.
    // The oldest of the recent entries is at index (N - expected_count) in the
    // original sequence.
    int start_idx = N - expected_count;
    for (int i = 0; i < expected_count; ++i) {
        int out_idx = i * 2;
        float expected_vib  = vibs[start_idx + i];
        float expected_temp = temps[start_idx + i];

        RC_ASSERT(flat[out_idx]     == expected_vib);
        RC_ASSERT(flat[out_idx + 1] == expected_temp);
    }

    // Remaining slots (if count < WINDOW_SIZE) should be zero
    for (int i = expected_count; i < config::WINDOW_SIZE; ++i) {
        int out_idx = i * 2;
        RC_ASSERT(flat[out_idx]     == 0.0f);
        RC_ASSERT(flat[out_idx + 1] == 0.0f);
    }
}
