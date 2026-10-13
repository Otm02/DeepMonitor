#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <cstring>
#include "nvs.h"
#include "ml.h"
#include "normalizer.h"

static rc::Gen<float> weightFloat() {
    return rc::gen::map(rc::gen::inRange(-10000, 10001), [](int v) {
        return static_cast<float>(v) / 1000.0f;
    });
}
static rc::Gen<float> positiveFloat() {
    return rc::gen::map(rc::gen::inRange(1, 10001), [](int v) {
        return static_cast<float>(v) / 1000.0f;
    });
}
static rc::Gen<float> learningRateGen() {
    return rc::gen::map(rc::gen::inRange(1, 10000), [](int v) {
        return static_cast<float>(v) / 100000.0f;
    });
}

static void fillWeights(ml::Weights& w) {
    for (auto& row : w.W1) for (auto& x : row) x = *weightFloat();
    for (auto& x : w.b1) x = *weightFloat();
    for (auto& row : w.W2) for (auto& x : row) x = *weightFloat();
    for (auto& x : w.b2) x = *weightFloat();
    for (auto& row : w.W3) for (auto& x : row) x = *weightFloat();
    for (auto& x : w.b3) x = *weightFloat();
    for (auto& row : w.W4) for (auto& x : row) x = *weightFloat();
    for (auto& x : w.b4) x = *weightFloat();
    w.learning_rate = *learningRateGen();
    w.normal_cycle_count = *rc::gen::inRange(0, 100000);
    w.anomaly_streak = *rc::gen::inRange(0, 100);
}

/**
 * **Validates: Requirements 7.4**
 * persist::save() then persist::load() reproduces weights and stats exactly.
 */
RC_GTEST_PROP(NVSPersistence, RoundTripPreservesData, ()) {
    persist::reset_mock();

    ml::Weights weights;
    fillWeights(weights);

    normalizer::Stats stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.count = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    stats.frozen = (*rc::gen::inRange(0, 2)) != 0;
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        stats.mean[i] = *weightFloat();
        stats.var[i] = *positiveFloat();
    }

    RC_ASSERT(persist::save(weights, stats));
    RC_ASSERT(persist::has_valid_data());

    ml::Weights out_weights;
    std::memset(&out_weights, 0, sizeof(out_weights));
    normalizer::Stats out_stats;
    std::memset(&out_stats, 0, sizeof(out_stats));

    RC_ASSERT(persist::load(out_weights, out_stats));

    RC_ASSERT(std::memcmp(&out_weights, &weights, sizeof(ml::Weights)) == 0);
    RC_ASSERT(std::memcmp(&out_stats, &stats, sizeof(normalizer::Stats)) == 0);
}

/**
 * **Validates: Requirements 7.1**
 * Time-based save triggers iff (count - last) >= NVS_SAVE_INTERVAL.
 */
RC_GTEST_PROP(NVSTimeBased, SaveTriggerMatchesIntervalCondition, ()) {
    const auto last_nvs_save_cycle = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    const auto offset = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    const uint32_t duty_cycle_count = last_nvs_save_cycle + offset;

    const bool result = persist::should_save_time_based(duty_cycle_count, last_nvs_save_cycle);
    const bool expected = (duty_cycle_count - last_nvs_save_cycle) >=
                          static_cast<uint32_t>(config::NVS_SAVE_INTERVAL);
    RC_ASSERT(result == expected);
}

/**
 * **Validates: Requirements 7.2**
 * Weight-shift save triggers iff compute_weight_shift >= WEIGHT_SHIFT_THRESHOLD.
 */
RC_GTEST_PROP(NVSWeightShift, SaveTriggerMatchesThresholdComparison, ()) {
    ml::Weights current;
    ml::Weights saved;
    fillWeights(current);
    fillWeights(saved);

    const bool result = persist::should_save_weight_shift(current, saved);
    const float shift = ml::compute_weight_shift(current, saved);
    const bool expected = shift >= config::WEIGHT_SHIFT_THRESHOLD;
    RC_ASSERT(result == expected);
}
