#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <cstring>
#include "nvs.h"
#include "ml.h"
#include "normalizer.h"

/**
 * Property 12: NVS Persistence Round Trip
 *
 * For any valid ML weights and normalizer stats, saving to NVS and then
 * loading should produce weights and stats equal to the originals.
 *
 * **Validates: Requirements 7.4**
 */

// Generator for bounded floats suitable for weight values
static rc::Gen<float> weightFloat() {
    return rc::gen::map(rc::gen::inRange(-10000, 10001), [](int v) {
        return static_cast<float>(v) / 1000.0f;  // [-10.0, 10.0]
    });
}

// Generator for positive floats suitable for variance values
static rc::Gen<float> positiveFloat() {
    return rc::gen::map(rc::gen::inRange(1, 10001), [](int v) {
        return static_cast<float>(v) / 1000.0f;  // (0.0, 10.0]
    });
}

// Generator for learning rate
static rc::Gen<float> learningRateGen() {
    return rc::gen::map(rc::gen::inRange(1, 10000), [](int v) {
        return static_cast<float>(v) / 100000.0f;  // (0.00001, 0.1]
    });
}

/**
 * **Validates: Requirements 7.4**
 *
 * Property: For random weights and stats, persist::save() followed by persist::load()
 * produces output identical to the originals.
 */
RC_GTEST_PROP(NVSPersistence, RoundTripPreservesData, ()) {
    // 1. Reset mock NVS state
    persist::reset_mock();

    // 2. Generate random ml::Weights
    ml::Weights weights;
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            weights.encoder[j][i] = *weightFloat();
        }
    }
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            weights.decoder[i][j] = *weightFloat();
        }
    }
    weights.learning_rate = *learningRateGen();
    weights.normal_cycle_count = *rc::gen::inRange(0, 100000);
    weights.anomaly_streak = *rc::gen::inRange(0, 100);

    // 3. Generate random normalizer::Stats
    normalizer::Stats stats;
    stats.mean_vib = *weightFloat();
    stats.var_vib = *positiveFloat();
    stats.mean_temp = *weightFloat();
    stats.var_temp = *positiveFloat();

    // 4. Save to NVS
    bool save_ok = persist::save(weights, stats);
    RC_ASSERT(save_ok);

    // 5. Verify has_valid_data() returns true
    RC_ASSERT(persist::has_valid_data());

    // 6. Create fresh output structs and load
    ml::Weights out_weights;
    std::memset(&out_weights, 0, sizeof(out_weights));
    normalizer::Stats out_stats;
    std::memset(&out_stats, 0, sizeof(out_stats));

    bool load_ok = persist::load(out_weights, out_stats);
    RC_ASSERT(load_ok);

    // 7. Verify all weight fields match exactly
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            RC_ASSERT(out_weights.encoder[j][i] == weights.encoder[j][i]);
        }
    }
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            RC_ASSERT(out_weights.decoder[i][j] == weights.decoder[i][j]);
        }
    }
    RC_ASSERT(out_weights.learning_rate == weights.learning_rate);
    RC_ASSERT(out_weights.normal_cycle_count == weights.normal_cycle_count);
    RC_ASSERT(out_weights.anomaly_streak == weights.anomaly_streak);

    // 8. Verify all stats fields match exactly
    RC_ASSERT(out_stats.mean_vib == stats.mean_vib);
    RC_ASSERT(out_stats.var_vib == stats.var_vib);
    RC_ASSERT(out_stats.mean_temp == stats.mean_temp);
    RC_ASSERT(out_stats.var_temp == stats.var_temp);
}

/**
 * Property 10: NVS Time-Based Save Trigger
 *
 * For any duty cycle count and last NVS save cycle (where duty_cycle_count >=
 * last_nvs_save_cycle), the NVS save should be triggered if and only if
 * (duty_cycle_count - last_nvs_save_cycle) >= NVS_SAVE_INTERVAL.
 *
 * **Validates: Requirements 7.1**
 */
RC_GTEST_PROP(NVSTimeBased, SaveTriggerMatchesIntervalCondition, ()) {
    // Generate random uint32_t values via inRange on non-negative ints
    const auto last_nvs_save_cycle = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    // Ensure duty_cycle_count >= last_nvs_save_cycle to avoid underflow
    const auto offset = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    const uint32_t duty_cycle_count = last_nvs_save_cycle + offset;

    const bool result = persist::should_save_time_based(duty_cycle_count, last_nvs_save_cycle);
    const bool expected = (duty_cycle_count - last_nvs_save_cycle) >=
                          static_cast<uint32_t>(config::NVS_SAVE_INTERVAL);

    RC_ASSERT(result == expected);
}

/**
 * Property 11: NVS Weight-Shift Save Trigger
 *
 * For any current weights and saved weights, the NVS save should be triggered
 * if and only if compute_weight_shift(current, saved) >= WEIGHT_SHIFT_THRESHOLD.
 *
 * **Validates: Requirements 7.2**
 */

// Generator for ml::Weights with bounded random values
static rc::Gen<ml::Weights> weightsGen() {
    return rc::gen::exec([]() {
        ml::Weights w;
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            for (int i = 0; i < config::INPUT_DIM; ++i) {
                w.encoder[j][i] = static_cast<float>(*rc::gen::inRange(-5000, 5001)) / 1000.0f;
            }
        }
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            for (int j = 0; j < config::LATENT_DIM; ++j) {
                w.decoder[i][j] = static_cast<float>(*rc::gen::inRange(-5000, 5001)) / 1000.0f;
            }
        }
        w.learning_rate = static_cast<float>(*rc::gen::inRange(1, 10000)) / 100000.0f;
        w.normal_cycle_count = *rc::gen::inRange(0, 100000);
        w.anomaly_streak = *rc::gen::inRange(0, 100);
        return w;
    });
}

RC_GTEST_PROP(NVSWeightShift, SaveTriggerMatchesThresholdComparison, ()) {
    const auto current = *weightsGen();
    const auto saved = *weightsGen();

    const bool result = persist::should_save_weight_shift(current, saved);

    // Independently compute the weight shift
    const float shift = ml::compute_weight_shift(current, saved);
    const bool expected = shift >= config::WEIGHT_SHIFT_THRESHOLD;

    RC_ASSERT(result == expected);
}
