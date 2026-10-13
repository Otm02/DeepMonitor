#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include "features.h"

// Generator for bounded floats in [-5, 5].
static rc::Gen<float> sampleFloat() {
    return rc::gen::map(rc::gen::inRange(-5000, 5001), [](int v) {
        return static_cast<float>(v) / 1000.0f;
    });
}

/**
 * Property: in-house radix-2 FFT magnitude matches a direct DFT.
 *
 * For any real input of FFT_SIZE samples, features::fft_magnitude must agree
 * (within floating-point tolerance) with the magnitude of the naive DFT for
 * every bin in [0, FFT_SIZE/2).
 */
RC_GTEST_PROP(FeaturesFFT, MatchesNaiveDFT, ()) {
    const int N = config::FFT_SIZE;
    float in[config::FFT_SIZE];
    for (int i = 0; i < N; ++i) in[i] = *sampleFloat();

    float mag[config::FFT_SIZE / 2];
    features::fft_magnitude(in, mag);

    for (int k = 0; k < N / 2; ++k) {
        double re = 0.0, im = 0.0;
        for (int n = 0; n < N; ++n) {
            double ang = -2.0 * M_PI * static_cast<double>(k) * static_cast<double>(n) /
                         static_cast<double>(N);
            re += static_cast<double>(in[n]) * std::cos(ang);
            im += static_cast<double>(in[n]) * std::sin(ang);
        }
        double expected = std::sqrt(re * re + im * im);
        double diff = std::fabs(static_cast<double>(mag[k]) - expected);
        // Tolerance scales with spectrum magnitude (float vs double, N=256).
        RC_ASSERT(diff <= 1e-2 + 1e-3 * expected);
    }
}

/**
 * Property: extracted features are always finite and the RMS / crest-factor
 * relationship holds for the AC (mean-removed) vibration signal.
 */
RC_GTEST_PROP(FeaturesExtract, FiniteAndConsistent, ()) {
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        vib[i] = *sampleFloat();
        temp[i] = 20.0f + *sampleFloat();
    }

    float feat[config::FEATURE_DIM];
    features::extract(vib, temp, feat);

    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        RC_ASSERT(std::isfinite(feat[i]));
    }

    // Independently compute RMS of the AC signal (feature index 0).
    double mean = 0.0;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) mean += vib[i];
    mean /= config::SAMPLE_COUNT;
    double sum_sq = 0.0;
    double peak = 0.0;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        double ac = vib[i] - mean;
        sum_sq += ac * ac;
        if (std::fabs(ac) > peak) peak = std::fabs(ac);
    }
    double rms = std::sqrt(sum_sq / config::SAMPLE_COUNT);

    RC_ASSERT(std::fabs(static_cast<double>(feat[0]) - rms) <= 1e-3 + 1e-3 * rms);

    // crest factor (index 2) == peak / rms when rms is non-trivial.
    if (rms > 1e-3) {
        double crest = peak / rms;
        RC_ASSERT(std::fabs(static_cast<double>(feat[2]) - crest) <= 1e-2 + 1e-2 * crest);
    }
}

/**
 * A constant (DC-only) signal has no AC energy: RMS, peak and band energies
 * must all be ~0.
 */
TEST(FeaturesExtract, ConstantSignalHasNoEnergy) {
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        vib[i] = 1.0f;     // pure DC
        temp[i] = 25.0f;
    }
    float feat[config::FEATURE_DIM];
    features::extract(vib, temp, feat);

    EXPECT_NEAR(feat[0], 0.0f, 1e-4f);  // rms
    EXPECT_NEAR(feat[1], 0.0f, 1e-4f);  // peak
    for (int k = 0; k < config::FREQ_BANDS; ++k) {
        EXPECT_NEAR(feat[6 + k], 0.0f, 1e-4f);  // band energies
    }
    EXPECT_NEAR(feat[config::FEATURE_DIM - 1], 25.0f, 1e-4f);  // temp mean
}
