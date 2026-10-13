#ifndef NORMALIZER_H
#define NORMALIZER_H

#include <cstdint>
#include "config.h"

// Adaptive per-feature normalizer.
//
// Maintains an exponentially-weighted running mean and variance for each
// element of the feature vector, and z-score normalizes new feature vectors
// against that baseline. Learning is skipped for one cycle after an anomaly
// (freeze) so a bad reading does not poison the baseline. The frozen flag
// lives in the struct so it survives deep sleep via RTC/NVS.
namespace normalizer {
    struct Stats {
        float mean[config::FEATURE_DIM];
        float var[config::FEATURE_DIM];
        uint32_t count;   // number of EMA updates applied
        bool frozen;      // skip the next update (set on anomaly)
    };

    // EMA update of mean/var from one feature vector. No-op (and clears frozen)
    // when frozen.
    void update(const float feat[config::FEATURE_DIM], Stats& stats, float alpha);

    // z-score: out[i] = (feat[i] - mean[i]) / sqrt(var[i] + eps).
    void normalize(const float feat[config::FEATURE_DIM], const Stats& stats,
                   float out[config::FEATURE_DIM]);

    // Mark the next update() to be skipped.
    void freeze(Stats& stats);
}

#endif // NORMALIZER_H
