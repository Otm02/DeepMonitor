#include "normalizer.h"
#include <cmath>

namespace normalizer {

// Floor on variance to avoid amplifying noise when a feature is nearly constant.
static const float EPS = 1e-6f;

void update(const float feat[config::FEATURE_DIM], Stats& stats, float alpha) {
    if (stats.frozen) {
        stats.frozen = false;
        return;
    }

    if (stats.count == 0) {
        for (int i = 0; i < config::FEATURE_DIM; ++i) {
            stats.mean[i] = feat[i];
            stats.var[i] = 0.0f;
        }
        stats.count = 1;
        return;
    }

    // West's incremental EWMA mean/variance.
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        float delta = feat[i] - stats.mean[i];
        stats.mean[i] += alpha * delta;
        stats.var[i] = (1.0f - alpha) * (stats.var[i] + alpha * delta * delta);
    }
    stats.count++;
}

void normalize(const float feat[config::FEATURE_DIM], const Stats& stats,
               float out[config::FEATURE_DIM]) {
    for (int i = 0; i < config::FEATURE_DIM; ++i) {
        out[i] = (feat[i] - stats.mean[i]) / std::sqrt(stats.var[i] + EPS);
    }
}

void freeze(Stats& stats) {
    stats.frozen = true;
}

} // namespace normalizer
