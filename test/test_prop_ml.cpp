#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <cstddef>
#include <cstring>
#include "ml.h"

// ── Generators ───────────────────────────────────────────────────────────────
static rc::Gen<float> smallFloat() {
    return rc::gen::map(rc::gen::inRange(-100, 101), [](int v) {
        return static_cast<float>(v) / 100.0f;  // [-1.0, 1.0]
    });
}
static rc::Gen<float> inputFloat() {
    return rc::gen::map(rc::gen::inRange(-200, 201), [](int v) {
        return static_cast<float>(v) / 100.0f;  // [-2.0, 2.0]
    });
}
static rc::Gen<float> thresholdGen() {
    return rc::gen::map(rc::gen::inRange(1, 10001), [](int v) {
        return static_cast<float>(v) / 10000.0f;
    });
}

static void randomizeWeights(ml::Weights& w) {
    for (auto& row : w.W1) for (auto& x : row) x = *smallFloat();
    for (auto& x : w.b1) x = *smallFloat();
    for (auto& row : w.W2) for (auto& x : row) x = *smallFloat();
    for (auto& x : w.b2) x = *smallFloat();
    for (auto& row : w.W3) for (auto& x : row) x = *smallFloat();
    for (auto& x : w.b3) x = *smallFloat();
    for (auto& row : w.W4) for (auto& x : row) x = *smallFloat();
    for (auto& x : w.b4) x = *smallFloat();
    w.learning_rate = config::LR_INITIAL;
    w.normal_cycle_count = 0;
    w.anomaly_streak = 0;
}

// Reference forward pass mirroring ml.cpp.
static float referenceReconError(const float* x, const ml::Weights& w) {
    const int F = config::AE_INPUT_DIM, H = config::AE_HIDDEN_DIM, L = config::AE_LATENT_DIM;
    float h1[config::AE_HIDDEN_DIM], z[config::AE_LATENT_DIM], h2[config::AE_HIDDEN_DIM], xr[config::AE_INPUT_DIM];
    for (int j = 0; j < H; ++j) {
        float s = w.b1[j];
        for (int i = 0; i < F; ++i) s += w.W1[j][i] * x[i];
        h1[j] = std::tanh(s);
    }
    for (int j = 0; j < L; ++j) {
        float s = w.b2[j];
        for (int i = 0; i < H; ++i) s += w.W2[j][i] * h1[i];
        z[j] = s;
    }
    for (int j = 0; j < H; ++j) {
        float s = w.b3[j];
        for (int i = 0; i < L; ++i) s += w.W3[j][i] * z[i];
        h2[j] = std::tanh(s);
    }
    float mse = 0.0f;
    for (int j = 0; j < F; ++j) {
        float s = w.b4[j];
        for (int i = 0; i < H; ++i) s += w.W4[j][i] * h2[i];
        float d = x[j] - s;
        mse += d * d;
    }
    return mse / static_cast<float>(F);
}

/**
 * **Validates: Requirements 4.3, 4.4**
 * infer() reconstruction error matches an independent forward pass and
 * is_anomaly == (error > threshold).
 */
RC_GTEST_PROP(AnomalyClassification, MatchesReconErrorVsThreshold, ()) {
    float input[config::AE_INPUT_DIM];
    for (int i = 0; i < config::AE_INPUT_DIM; ++i) input[i] = *inputFloat();

    ml::Weights w;
    randomizeWeights(w);

    float threshold = *thresholdGen();
    ml::InferenceResult result = ml::infer(input, w, threshold);

    float expected = referenceReconError(input, w);
    float diff = std::fabs(result.reconstruction_error - expected);
    RC_ASSERT(diff <= 1e-5f + 1e-4f * std::fabs(expected));
    RC_ASSERT(result.is_anomaly == (expected > threshold));
}

/**
 * **Validates: Requirements 4.2, 4.5**
 * A normal cycle updates at least one weight.
 */
RC_GTEST_PROP(WeightUpdateConditioned, NormalCycleChangesWeights, ()) {
    float input[config::AE_INPUT_DIM];
    for (int i = 0; i < config::AE_INPUT_DIM; ++i) input[i] = *inputFloat();

    ml::Weights w;
    randomizeWeights(w);

    ml::InferenceResult result = ml::infer(input, w, 1e10f);  // force normal
    RC_ASSERT(!result.is_anomaly);

    ml::Weights before = w;
    ml::update_weights(input, result, w);

    RC_ASSERT(std::memcmp(&w, &before, offsetof(ml::Weights, learning_rate)) != 0);
}

/**
 * **Validates: Requirements 4.2, 4.5**
 * An anomalous cycle leaves every weight/bias unchanged.
 */
RC_GTEST_PROP(WeightUpdateConditioned, AnomalyCycleKeepsWeightsUnchanged, ()) {
    float input[config::AE_INPUT_DIM];
    for (int i = 0; i < config::AE_INPUT_DIM; ++i) input[i] = *inputFloat();

    ml::Weights w;
    randomizeWeights(w);
    w.normal_cycle_count = *rc::gen::inRange(0, 1000);
    w.anomaly_streak = *rc::gen::inRange(0, 9);

    ml::InferenceResult result = ml::infer(input, w, config::ANOMALY_THRESHOLD);
    result.is_anomaly = true;

    ml::Weights before = w;
    ml::update_weights(input, result, w);

    RC_ASSERT(std::memcmp(&w, &before, offsetof(ml::Weights, learning_rate)) == 0);
}

/**
 * **Validates: Requirements 4.6**
 * Learning rate decays as LR_INITIAL / (1 + DECAY * normal_cycle_count).
 */
RC_GTEST_PROP(LearningRateDecay, MonotonicallyDecreasesOnNormalCycles, ()) {
    ml::Weights w;
    ml::init_weights_random(w);

    int N = *rc::gen::inRange(1, 51);
    float input[config::AE_INPUT_DIM];
    for (int i = 0; i < config::AE_INPUT_DIM; ++i) input[i] = *inputFloat();

    float prev_lr = w.learning_rate;
    for (int c = 0; c < N; ++c) {
        ml::InferenceResult result = ml::infer(input, w, 1e10f);
        RC_ASSERT(!result.is_anomaly);
        ml::update_weights(input, result, w);
        RC_ASSERT(w.learning_rate < prev_lr);
        prev_lr = w.learning_rate;
    }

    float expected = config::LR_INITIAL / (1.0f + config::LR_DECAY_RATE * static_cast<float>(N));
    RC_ASSERT(std::fabs(w.learning_rate - expected) <= 1e-8f + 1e-4f * std::fabs(expected));
}

/**
 * **Validates: Requirements 4.6**
 * LR resets to LR_INITIAL after LR_RESET_STREAK consecutive anomalies.
 */
RC_GTEST_PROP(LearningRateReset, ResetsAfterAnomalyStreak, ()) {
    ml::Weights w;
    ml::init_weights_random(w);

    int pre_cycles = *rc::gen::inRange(5, 30);
    float input[config::AE_INPUT_DIM];
    for (int i = 0; i < config::AE_INPUT_DIM; ++i) input[i] = *inputFloat();
    for (int i = 0; i < pre_cycles; ++i) {
        ml::InferenceResult result = ml::infer(input, w, 1e10f);
        ml::update_weights(input, result, w);
    }
    RC_ASSERT(w.learning_rate < config::LR_INITIAL);

    for (int i = 0; i < config::LR_RESET_STREAK; ++i) {
        ml::InferenceResult anomaly_result;
        anomaly_result.is_anomaly = true;
        anomaly_result.reconstruction_error = 1.0f;
        for (int j = 0; j < config::AE_LATENT_DIM; ++j) anomaly_result.latent[j] = 0.0f;

        if (i < config::LR_RESET_STREAK - 1) {
            float lr_before = w.learning_rate;
            ml::update_weights(input, anomaly_result, w);
            RC_ASSERT(w.learning_rate == lr_before);
        } else {
            ml::update_weights(input, anomaly_result, w);
        }
    }

    RC_ASSERT(std::fabs(w.learning_rate - config::LR_INITIAL) < 1e-8f);
    RC_ASSERT(w.anomaly_streak == 0);
}

/**
 * Backprop sanity: repeatedly training on one normal sample reduces its
 * reconstruction error (gradient descent actually descends).
 */
TEST(Autoencoder, TrainingReducesReconstructionError) {
    ml::Weights w;
    ml::init_weights_random(w);

    float input[config::AE_INPUT_DIM];
    for (int i = 0; i < config::AE_INPUT_DIM; ++i) {
        input[i] = std::sin(0.3f * static_cast<float>(i)) + 0.2f;
    }

    float initial = ml::infer(input, w, 1e10f).reconstruction_error;
    for (int c = 0; c < 200; ++c) {
        ml::InferenceResult r = ml::infer(input, w, 1e10f);
        ml::update_weights(input, r, w);
    }
    float final_err = ml::infer(input, w, 1e10f).reconstruction_error;

    EXPECT_LT(final_err, initial);
}

/**
 * Adaptive threshold: during warmup nothing is flagged; afterwards the
 * threshold tracks mean + k*std of the recon history and respects the floor.
 */
TEST(AdaptiveThreshold, WarmupThenTracksStatistics) {
    ml::ReconTracker t;
    std::memset(&t, 0, sizeof(t));

    // During warmup the threshold is effectively infinite.
    ml::recon_push(t, 0.01f);
    EXPECT_GT(ml::adaptive_threshold(t, 3.0f, config::ANOMALY_THRESHOLD), 1e20f);

    // Fill with a steady low error.
    for (int i = 0; i < config::RECON_HISTORY; ++i) ml::recon_push(t, 0.02f);
    float thr = ml::adaptive_threshold(t, 3.0f, 0.0f);
    // Zero variance -> threshold == mean == 0.02 (floor 0).
    EXPECT_NEAR(thr, 0.02f, 1e-3f);

    // Floor dominates when it is higher than mean + k*std.
    float floored = ml::adaptive_threshold(t, 3.0f, 0.5f);
    EXPECT_NEAR(floored, 0.5f, 1e-6f);
}
