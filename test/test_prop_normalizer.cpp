#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include "normalizer.h"

/**
 * Property 2: EMA Update Formula
 *
 * For any previous running mean, previous running variance, new batch of
 * samples, and smoothing factor α in (0, 1), the updated mean should equal
 * (1 - α) * old_mean + α * batch_mean and the updated variance should equal
 * (1 - α) * old_var + α * batch_var.
 *
 * **Validates: Requirements 2.2**
 */

// Generator for bounded floats in [-50, 50] to avoid numerical issues
static rc::Gen<float> boundedFloat() {
    return rc::gen::map(rc::gen::inRange(-5000, 5001), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}

// Generator for positive floats in (0.01, 10] for variance
static rc::Gen<float> positiveFloat() {
    return rc::gen::map(rc::gen::inRange(1, 1001), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}

// Generator for alpha in (0.01, 0.99) to stay away from boundaries
static rc::Gen<float> alphaGen() {
    return rc::gen::map(rc::gen::inRange(1, 100), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}

// Helper: compute batch mean from an array of SAMPLE_COUNT floats
static float batchMean(const float arr[config::SAMPLE_COUNT]) {
    float sum = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        sum += arr[i];
    }
    return sum / static_cast<float>(config::SAMPLE_COUNT);
}

// Helper: compute batch variance from an array of SAMPLE_COUNT floats
static float batchVar(const float arr[config::SAMPLE_COUNT], float mean) {
    float sum_sq = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        float d = arr[i] - mean;
        sum_sq += d * d;
    }
    return sum_sq / static_cast<float>(config::SAMPLE_COUNT);
}

/**
 * **Validates: Requirements 2.2**
 *
 * Property: After calling normalizer::update() with a batch of samples and
 * smoothing factor α, the resulting stats match the EMA formula:
 *   mean_new = (1 - α) * old_mean + α * batch_mean
 *   var_new  = (1 - α) * old_var  + α * batch_var
 */
RC_GTEST_PROP(EMAUpdateFormula, MeanAndVarianceFollowEMA, ()) {
    // Generate random initial stats
    float old_mean_vib  = *boundedFloat();
    float old_var_vib   = *positiveFloat();
    float old_mean_temp = *boundedFloat();
    float old_var_temp  = *positiveFloat();
    float alpha         = *alphaGen();

    // Generate random batch of 500 vibration and temperature samples
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        vib[i]  = *boundedFloat();
        temp[i] = *boundedFloat();
    }

    // Compute expected batch statistics
    float exp_batch_mean_vib  = batchMean(vib);
    float exp_batch_var_vib   = batchVar(vib, exp_batch_mean_vib);
    float exp_batch_mean_temp = batchMean(temp);
    float exp_batch_var_temp  = batchVar(temp, exp_batch_mean_temp);

    // Compute expected EMA results
    float exp_mean_vib  = (1.0f - alpha) * old_mean_vib  + alpha * exp_batch_mean_vib;
    float exp_var_vib   = (1.0f - alpha) * old_var_vib   + alpha * exp_batch_var_vib;
    float exp_mean_temp = (1.0f - alpha) * old_mean_temp + alpha * exp_batch_mean_temp;
    float exp_var_temp  = (1.0f - alpha) * old_var_temp  + alpha * exp_batch_var_temp;

    // Set up stats and call normalizer::update()
    normalizer::Stats stats;
    stats.mean_vib  = old_mean_vib;
    stats.var_vib   = old_var_vib;
    stats.mean_temp = old_mean_temp;
    stats.var_temp  = old_var_temp;

    normalizer::update(vib, temp, stats, alpha);

    // Verify EMA formula with tolerance for floating-point precision
    const float tol = 1e-3f;
    RC_ASSERT(std::abs(stats.mean_vib  - exp_mean_vib)  < tol);
    RC_ASSERT(std::abs(stats.var_vib   - exp_var_vib)   < tol);
    RC_ASSERT(std::abs(stats.mean_temp - exp_mean_temp) < tol);
    RC_ASSERT(std::abs(stats.var_temp  - exp_var_temp)  < tol);
}

/**
 * Property 3: Normalization Produces Zero-Mean Unit-Variance Output
 *
 * For any batch of samples drawn from a distribution with the same mean and
 * variance as the normalizer stats, the normalized output should have a mean
 * within ε of zero and a variance within ε of one.
 *
 * **Validates: Requirements 2.3**
 */
RC_GTEST_PROP(NormalizationOutput, ProducesZeroMeanUnitVariance, ()) {
    // Generate stats with positive variance (range [0.1, 10.0])
    float mean_vib  = *boundedFloat();
    float var_vib   = *positiveFloat();
    float mean_temp = *boundedFloat();
    float var_temp  = *positiveFloat();

    // Ensure variance is large enough so sqrt(var/(var+eps)) ≈ 1
    RC_PRE(var_vib >= 0.1f);
    RC_PRE(var_temp >= 0.1f);

    normalizer::Stats stats;
    stats.mean_vib  = mean_vib;
    stats.var_vib   = var_vib;
    stats.mean_temp = mean_temp;
    stats.var_temp  = var_temp;

    float std_vib  = std::sqrt(var_vib);
    float std_temp = std::sqrt(var_temp);

    // Generate batch: each sample = mean + std * offset, where offsets are
    // drawn uniformly from [-1, 1]. This gives a batch whose population mean
    // is close to stats.mean and whose population variance is close to
    // std^2 / 3 ≈ var / 3. Instead, we use a simpler approach: generate
    // offsets from a standard-ish distribution and then set the stats to
    // match the actual batch statistics exactly.
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];

    // Generate raw offsets
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        vib[i]  = *boundedFloat();
        temp[i] = *boundedFloat();
    }

    // Compute actual batch mean and variance
    float sum_vib = 0.0f, sum_temp = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        sum_vib  += vib[i];
        sum_temp += temp[i];
    }
    float actual_mean_vib  = sum_vib  / config::SAMPLE_COUNT;
    float actual_mean_temp = sum_temp / config::SAMPLE_COUNT;

    float sum_sq_vib = 0.0f, sum_sq_temp = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        float dv = vib[i] - actual_mean_vib;
        float dt = temp[i] - actual_mean_temp;
        sum_sq_vib  += dv * dv;
        sum_sq_temp += dt * dt;
    }
    float actual_var_vib  = sum_sq_vib  / config::SAMPLE_COUNT;
    float actual_var_temp = sum_sq_temp / config::SAMPLE_COUNT;

    // Skip degenerate batches with near-zero variance
    RC_PRE(actual_var_vib  > 0.01f);
    RC_PRE(actual_var_temp > 0.01f);

    // Set stats to match the actual batch statistics so normalization
    // should produce exactly zero-mean, unit-variance output
    stats.mean_vib  = actual_mean_vib;
    stats.var_vib   = actual_var_vib;
    stats.mean_temp = actual_mean_temp;
    stats.var_temp  = actual_var_temp;

    // Normalize
    float norm_vib[config::SAMPLE_COUNT];
    float norm_temp[config::SAMPLE_COUNT];
    normalizer::normalize(vib, temp, stats, norm_vib, norm_temp);

    // Compute normalized output mean
    float norm_sum_vib = 0.0f, norm_sum_temp = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        norm_sum_vib  += norm_vib[i];
        norm_sum_temp += norm_temp[i];
    }
    float norm_mean_vib  = norm_sum_vib  / config::SAMPLE_COUNT;
    float norm_mean_temp = norm_sum_temp / config::SAMPLE_COUNT;

    // Compute normalized output variance
    float norm_sq_vib = 0.0f, norm_sq_temp = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        float dv = norm_vib[i] - norm_mean_vib;
        float dt = norm_temp[i] - norm_mean_temp;
        norm_sq_vib  += dv * dv;
        norm_sq_temp += dt * dt;
    }
    float norm_var_vib  = norm_sq_vib  / config::SAMPLE_COUNT;
    float norm_var_temp = norm_sq_temp / config::SAMPLE_COUNT;

    // The normalized mean should be ≈ 0
    // Since stats.mean == batch mean, (x - mean) sums to 0
    const float eps_mean = 1e-3f;
    RC_ASSERT(std::abs(norm_mean_vib)  < eps_mean);
    RC_ASSERT(std::abs(norm_mean_temp) < eps_mean);

    // The normalized variance should be ≈ var / (var + 1e-6) ≈ 1
    // For var >= 0.01, var/(var+1e-6) is extremely close to 1
    const float eps_var = 1e-2f;
    RC_ASSERT(std::abs(norm_var_vib  - 1.0f) < eps_var);
    RC_ASSERT(std::abs(norm_var_temp - 1.0f) < eps_var);
}

/**
 * Property 4: Normalizer Freezes on Anomaly
 *
 * For any normalizer stats and any batch of samples, if freeze() is called
 * before update(), the normalizer stats after update() should be identical
 * to the stats before the call — the EMA update is skipped entirely.
 *
 * **Validates: Requirements 2.4**
 */
RC_GTEST_PROP(NormalizerFreeze, StatsUnchangedWhenFrozen, ()) {
    // Generate random initial stats
    float mean_vib  = *boundedFloat();
    float var_vib   = *positiveFloat();
    float mean_temp = *boundedFloat();
    float var_temp  = *positiveFloat();
    float alpha     = *alphaGen();

    // Generate a random batch of 500 vibration and temperature samples
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        vib[i]  = *boundedFloat();
        temp[i] = *boundedFloat();
    }

    // Set up stats
    normalizer::Stats stats;
    stats.mean_vib  = mean_vib;
    stats.var_vib   = var_vib;
    stats.mean_temp = mean_temp;
    stats.var_temp  = var_temp;

    // Save a copy of the stats before update
    normalizer::Stats saved = stats;

    // Freeze the normalizer (simulates anomaly detected)
    normalizer::freeze();

    // Call update — should be a no-op because frozen flag is set
    normalizer::update(vib, temp, stats, alpha);

    // Verify stats are IDENTICAL to the saved copy
    RC_ASSERT(stats.mean_vib  == saved.mean_vib);
    RC_ASSERT(stats.var_vib   == saved.var_vib);
    RC_ASSERT(stats.mean_temp == saved.mean_temp);
    RC_ASSERT(stats.var_temp  == saved.var_temp);
}
