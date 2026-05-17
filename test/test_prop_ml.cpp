#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <cstring>
#include "ml.h"

/**
 * Property 7: Anomaly Classification Matches MSE vs Threshold
 *
 * For any input vector and weight matrices, the infer function should return
 * is_anomaly == true if and only if the mean squared error between the input
 * and the reconstructed output exceeds the anomaly threshold.
 *
 * **Validates: Requirements 4.3, 4.4**
 */

// Generator for small bounded floats to avoid numerical overflow
static rc::Gen<float> smallFloat() {
    return rc::gen::map(rc::gen::inRange(-100, 101), [](int v) {
        return static_cast<float>(v) / 100.0f;  // [-1.0, 1.0]
    });
}

// Generator for small input values
static rc::Gen<float> inputFloat() {
    return rc::gen::map(rc::gen::inRange(-200, 201), [](int v) {
        return static_cast<float>(v) / 100.0f;  // [-2.0, 2.0]
    });
}

// Generator for positive threshold
static rc::Gen<float> thresholdGen() {
    return rc::gen::map(rc::gen::inRange(1, 10001), [](int v) {
        return static_cast<float>(v) / 10000.0f;  // (0.0001, 1.0]
    });
}

/**
 * **Validates: Requirements 4.3, 4.4**
 *
 * Property: For random inputs, weights, and threshold, ml::infer() returns
 * reconstruction_error matching the independently computed MSE, and
 * is_anomaly == (MSE > threshold).
 */
RC_GTEST_PROP(AnomalyClassification, MatchesMSEvsThreshold, ()) {
    // 1. Generate random input vector of INPUT_DIM floats
    float input[config::INPUT_DIM];
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        input[i] = *inputFloat();
    }

    // 2. Generate random Weights with small values
    ml::Weights w;
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            w.encoder[j][i] = *smallFloat();
        }
    }
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            w.decoder[i][j] = *smallFloat();
        }
    }
    w.learning_rate = 0.01f;
    w.normal_cycle_count = 0;
    w.anomaly_streak = 0;

    // 3. Generate a random threshold > 0
    float threshold = *thresholdGen();

    // 4. Call ml::infer()
    ml::InferenceResult result = ml::infer(input, w, threshold);

    // 5. Independently compute expected MSE
    // Encode: latent[j] = Σ_i encoder[j][i] * input[i]
    float latent[config::LATENT_DIM];
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        float sum = 0.0f;
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            sum += w.encoder[j][i] * input[i];
        }
        latent[j] = sum;
    }

    // Decode: reconstructed[i] = Σ_j decoder[i][j] * latent[j]
    float reconstructed[config::INPUT_DIM];
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        float sum = 0.0f;
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            sum += w.decoder[i][j] * latent[j];
        }
        reconstructed[i] = sum;
    }

    // MSE = (1/INPUT_DIM) * Σ_i (input[i] - reconstructed[i])²
    float expected_mse = 0.0f;
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        float diff = input[i] - reconstructed[i];
        expected_mse += diff * diff;
    }
    expected_mse /= static_cast<float>(config::INPUT_DIM);

    // 6. Verify reconstruction_error ≈ expected MSE (within floating-point tolerance)
    float rel_tol = 1e-4f;
    float abs_tol = 1e-6f;
    float err_diff = std::fabs(result.reconstruction_error - expected_mse);
    RC_ASSERT(err_diff <= abs_tol + rel_tol * std::fabs(expected_mse));

    // 7. Verify is_anomaly == (expected MSE > threshold)
    bool expected_anomaly = (expected_mse > threshold);
    RC_ASSERT(result.is_anomaly == expected_anomaly);
}

/**
 * Property 6: Weight Update Conditioned on Anomaly Status
 *
 * For any input vector, weights, and inference result: if the cycle is normal
 * (not anomalous), calling update_weights should produce weights different from
 * the input weights; if the cycle is anomalous, the weights should remain unchanged.
 *
 * **Validates: Requirements 4.2, 4.5**
 */

// Generator for non-zero floats in a small range to ensure non-trivial updates
static rc::Gen<float> nonZeroSmallFloat() {
    return rc::gen::suchThat(
        rc::gen::map(rc::gen::inRange(-100, 101), [](int v) {
            return static_cast<float>(v) / 100.0f;
        }),
        [](float v) { return std::fabs(v) > 0.01f; }
    );
}

RC_GTEST_PROP(WeightUpdateConditioned, NormalCycleChangesWeights, ()) {
    // 1. Generate non-trivial input vector
    float input[config::INPUT_DIM];
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        input[i] = *nonZeroSmallFloat();
    }

    // 2. Generate random weights with non-zero values
    ml::Weights w;
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            w.encoder[j][i] = *nonZeroSmallFloat();
        }
    }
    // Set decoder = transpose of encoder (tied weights)
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            w.decoder[i][j] = w.encoder[j][i];
        }
    }
    w.learning_rate = config::LR_INITIAL;
    w.normal_cycle_count = 0;
    w.anomaly_streak = 0;

    // 3. Run infer() to get valid latent values
    ml::InferenceResult result = ml::infer(input, w, config::ANOMALY_THRESHOLD);

    // 4. Force normal cycle
    result.is_anomaly = false;

    // 5. Save a copy of encoder weights before update
    float encoder_before[config::LATENT_DIM][config::INPUT_DIM];
    std::memcpy(encoder_before, w.encoder, sizeof(encoder_before));

    // 6. Call update_weights with normal cycle
    ml::update_weights(input, result, w);

    // 7. Verify at least one encoder weight changed
    bool any_changed = false;
    for (int j = 0; j < config::LATENT_DIM && !any_changed; ++j) {
        for (int i = 0; i < config::INPUT_DIM && !any_changed; ++i) {
            if (w.encoder[j][i] != encoder_before[j][i]) {
                any_changed = true;
            }
        }
    }
    RC_ASSERT(any_changed);
}

RC_GTEST_PROP(WeightUpdateConditioned, AnomalyCycleKeepsWeightsUnchanged, ()) {
    // 1. Generate random input vector
    float input[config::INPUT_DIM];
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        input[i] = *inputFloat();
    }

    // 2. Generate random weights
    ml::Weights w;
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            w.encoder[j][i] = *smallFloat();
        }
    }
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            w.decoder[i][j] = *smallFloat();
        }
    }
    w.learning_rate = config::LR_INITIAL;
    w.normal_cycle_count = *rc::gen::inRange(0, 1000);
    w.anomaly_streak = *rc::gen::inRange(0, 9);  // Below reset threshold

    // 3. Run infer() to get valid latent values
    ml::InferenceResult result = ml::infer(input, w, config::ANOMALY_THRESHOLD);

    // 4. Force anomaly cycle
    result.is_anomaly = true;

    // 5. Save a copy of encoder weights before update
    float encoder_before[config::LATENT_DIM][config::INPUT_DIM];
    std::memcpy(encoder_before, w.encoder, sizeof(encoder_before));

    // 6. Call update_weights with anomaly cycle
    ml::update_weights(input, result, w);

    // 7. Verify all encoder weights are identical
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            RC_ASSERT(w.encoder[j][i] == encoder_before[j][i]);
        }
    }
}

/**
 * Property 8: Learning Rate Decay and Reset
 *
 * For any sequence of consecutive normal duty cycles, the learning rate should
 * monotonically decrease according to lr = lr_initial / (1 + decay_rate * normal_cycle_count).
 * For any sequence of consecutive anomalous cycles exceeding the reset threshold,
 * the learning rate should reset to lr_initial.
 *
 * **Validates: Requirements 4.6**
 */

RC_GTEST_PROP(LearningRateDecay, MonotonicallyDecreasesOnNormalCycles, ()) {
    // 1. Initialize fresh weights
    ml::Weights w;
    ml::init_weights_random(w);

    // 2. Generate a random number of normal cycles N (1 to 50)
    int N = *rc::gen::inRange(1, 51);

    // 3. Create a non-trivial input
    float input[config::INPUT_DIM];
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        input[i] = *nonZeroSmallFloat();
    }

    float prev_lr = w.learning_rate;

    // 4. Run N normal cycles
    for (int cycle = 0; cycle < N; ++cycle) {
        // Use a very high threshold so infer() always returns is_anomaly=false
        ml::InferenceResult result = ml::infer(input, w, 1e10f);
        RC_ASSERT(!result.is_anomaly);

        ml::update_weights(input, result, w);

        // 5. Verify learning rate is monotonically decreasing
        RC_ASSERT(w.learning_rate < prev_lr);
        prev_lr = w.learning_rate;
    }

    // 6. After N normal cycles, verify: w.learning_rate ≈ LR_INITIAL / (1 + LR_DECAY_RATE * N)
    float expected_lr = config::LR_INITIAL / (1.0f + config::LR_DECAY_RATE * static_cast<float>(N));
    float rel_tol = 1e-4f;
    float abs_tol = 1e-8f;
    float lr_diff = std::fabs(w.learning_rate - expected_lr);
    RC_ASSERT(lr_diff <= abs_tol + rel_tol * std::fabs(expected_lr));
}

RC_GTEST_PROP(LearningRateReset, ResetsAfterAnomalyStreak, ()) {
    // 1. Initialize fresh weights
    ml::Weights w;
    ml::init_weights_random(w);

    // First decay the learning rate with some normal cycles so it's not at LR_INITIAL
    int pre_cycles = *rc::gen::inRange(5, 30);
    float input[config::INPUT_DIM];
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        input[i] = *nonZeroSmallFloat();
    }
    for (int i = 0; i < pre_cycles; ++i) {
        ml::InferenceResult result = ml::infer(input, w, 1e10f);
        ml::update_weights(input, result, w);
    }
    // Confirm LR has decayed below initial
    RC_ASSERT(w.learning_rate < config::LR_INITIAL);

    // 2. Simulate LR_RESET_STREAK consecutive anomalous cycles
    for (int i = 0; i < config::LR_RESET_STREAK; ++i) {
        ml::InferenceResult anomaly_result;
        anomaly_result.is_anomaly = true;
        anomaly_result.reconstruction_error = 1.0f;
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            anomaly_result.latent[j] = 0.0f;
        }

        // Before the last anomaly cycle, LR should not yet be reset
        if (i < config::LR_RESET_STREAK - 1) {
            float lr_before = w.learning_rate;
            ml::update_weights(input, anomaly_result, w);
            // LR should not have been reset yet (streak not reached)
            RC_ASSERT(w.learning_rate == lr_before);
        } else {
            // The LR_RESET_STREAK-th anomaly triggers the reset
            ml::update_weights(input, anomaly_result, w);
        }
    }

    // 3. Verify that after exactly LR_RESET_STREAK anomalous cycles, LR is reset to LR_INITIAL
    RC_ASSERT(std::fabs(w.learning_rate - config::LR_INITIAL) < 1e-8f);

    // 4. Verify anomaly_streak is reset to 0
    RC_ASSERT(w.anomaly_streak == 0);
}
