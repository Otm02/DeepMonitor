#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <cstring>
#include "normalizer.h"

static const float EPS = 1e-6f;

static rc::Gen<float> boundedFloat() {
    return rc::gen::map(rc::gen::inRange(-5000, 5001), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}
static rc::Gen<float> positiveFloat() {
    return rc::gen::map(rc::gen::inRange(1, 1001), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}
static rc::Gen<float> alphaGen() {
    return rc::gen::map(rc::gen::inRange(1, 100), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}

/**
 * **Validates: Requirements 2.2**
 * Per-feature EWMA update: for an already-initialized Stats (count > 0),
 *   mean' = mean + alpha*(feat - mean)
 *   var'  = (1 - alpha) * (var + alpha*(feat - mean)^2)
 */
RC_GTEST_PROP(EMAUpdateFormula, MeanAndVarianceFollowEWMA, ()) {
    float alpha = *alphaGen();

    normalizer::Stats stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.count = 5;       // already initialized
    stats.frozen = false;

    float feat[config::FEATURE_DIM];
    float old_mean[config::FEATURE_DIM];
    float old_var[config::FEATURE_DIM];
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        stats.mean[i] = *boundedFloat();
        stats.var[i]  = *positiveFloat();
        feat[i] = *boundedFloat();
        old_mean[i] = stats.mean[i];
        old_var[i] = stats.var[i];
    }

    normalizer::update(feat, stats, alpha);

    const float tol = 1e-3f;
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        float delta = feat[i] - old_mean[i];
        float exp_mean = old_mean[i] + alpha * delta;
        float exp_var = (1.0f - alpha) * (old_var[i] + alpha * delta * delta);
        RC_ASSERT(std::fabs(stats.mean[i] - exp_mean) < tol + 1e-3f * std::fabs(exp_mean));
        RC_ASSERT(std::fabs(stats.var[i] - exp_var) < tol + 1e-3f * std::fabs(exp_var));
    }
}

/**
 * **Validates: Requirements 2.3**
 * Normalization is the z-score: out[i] = (feat[i]-mean[i]) / sqrt(var[i]+eps).
 */
RC_GTEST_PROP(NormalizationOutput, IsZScore, ()) {
    normalizer::Stats stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.count = 10;

    float feat[config::FEATURE_DIM];
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        stats.mean[i] = *boundedFloat();
        stats.var[i]  = *positiveFloat();
        feat[i] = *boundedFloat();
    }

    float out[config::FEATURE_DIM];
    normalizer::normalize(feat, stats, out);

    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        float expected = (feat[i] - stats.mean[i]) / std::sqrt(stats.var[i] + EPS);
        RC_ASSERT(std::fabs(out[i] - expected) < 1e-3f + 1e-3f * std::fabs(expected));
    }
}

/**
 * **Validates: Requirements 2.4**
 * When frozen, update() is a no-op for mean/var and clears the frozen flag.
 */
RC_GTEST_PROP(NormalizerFreeze, StatsUnchangedWhenFrozen, ()) {
    float alpha = *alphaGen();

    normalizer::Stats stats;
    std::memset(&stats, 0, sizeof(stats));
    stats.count = 7;
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        stats.mean[i] = *boundedFloat();
        stats.var[i]  = *positiveFloat();
    }
    normalizer::freeze(stats);

    normalizer::Stats saved = stats;

    float feat[config::FEATURE_DIM];
    for (int i = 0; i < config::FEATURE_DIM; ++i) feat[i] = *boundedFloat();

    normalizer::update(feat, stats, alpha);

    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        RC_ASSERT(stats.mean[i] == saved.mean[i]);
        RC_ASSERT(stats.var[i] == saved.var[i]);
    }
    RC_ASSERT(stats.frozen == false);   // freeze lasts exactly one cycle
    RC_ASSERT(stats.count == saved.count);
}

/**
 * First update (count == 0) seeds the mean with the sample and zero variance.
 */
TEST(NormalizerInit, FirstUpdateSeedsMean) {
    normalizer::Stats stats;
    std::memset(&stats, 0, sizeof(stats));

    float feat[config::FEATURE_DIM];
    for (int i = 0; i < config::FEATURE_DIM; ++i) feat[i] = 1.5f * static_cast<float>(i) - 3.0f;

    normalizer::update(feat, stats, 0.1f);

    EXPECT_EQ(stats.count, 1u);
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        EXPECT_FLOAT_EQ(stats.mean[i], feat[i]);
        EXPECT_FLOAT_EQ(stats.var[i], 0.0f);
    }
}
