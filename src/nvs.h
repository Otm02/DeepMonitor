#ifndef NVS_H
#define NVS_H

#include <cstdint>
#include "ml.h"
#include "normalizer.h"

namespace persist {
    bool save(const ml::Weights& weights, const normalizer::Stats& stats);
    bool load(ml::Weights& weights, normalizer::Stats& stats);
    bool has_valid_data();

    // NVS save trigger helpers (pure logic, no hardware dependency)
    bool should_save_time_based(uint32_t duty_cycle_count, uint32_t last_nvs_save_cycle);
    bool should_save_weight_shift(const ml::Weights& current, const ml::Weights& saved);

#ifdef HOST_TEST
    void reset_mock();
#endif
}

#endif // NVS_H
