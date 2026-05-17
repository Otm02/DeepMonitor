#include "normalizer.h"
#include <cmath>

namespace normalizer {

static bool frozen = false;

void update(const float vib[config::SAMPLE_COUNT], const float temp[config::SAMPLE_COUNT], Stats& stats, float alpha) {
    if (frozen) {
        frozen = false;
        return;
    }

    // Compute batch mean for vibration and temperature
    float sum_vib = 0.0f;
    float sum_temp = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        sum_vib += vib[i];
        sum_temp += temp[i];
    }
    float batch_mean_vib = sum_vib / config::SAMPLE_COUNT;
    float batch_mean_temp = sum_temp / config::SAMPLE_COUNT;

    // Compute batch variance for vibration and temperature
    float sum_sq_vib = 0.0f;
    float sum_sq_temp = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        float dv = vib[i] - batch_mean_vib;
        float dt = temp[i] - batch_mean_temp;
        sum_sq_vib += dv * dv;
        sum_sq_temp += dt * dt;
    }
    float batch_var_vib = sum_sq_vib / config::SAMPLE_COUNT;
    float batch_var_temp = sum_sq_temp / config::SAMPLE_COUNT;

    // EMA update
    stats.mean_vib = (1.0f - alpha) * stats.mean_vib + alpha * batch_mean_vib;
    stats.var_vib  = (1.0f - alpha) * stats.var_vib  + alpha * batch_var_vib;
    stats.mean_temp = (1.0f - alpha) * stats.mean_temp + alpha * batch_mean_temp;
    stats.var_temp  = (1.0f - alpha) * stats.var_temp  + alpha * batch_var_temp;
}

void normalize(const float vib[config::SAMPLE_COUNT], const float temp[config::SAMPLE_COUNT],
               const Stats& stats,
               float norm_vib[config::SAMPLE_COUNT], float norm_temp[config::SAMPLE_COUNT]) {
    const float eps = 1e-6f;
    float inv_std_vib = 1.0f / std::sqrt(stats.var_vib + eps);
    float inv_std_temp = 1.0f / std::sqrt(stats.var_temp + eps);

    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        norm_vib[i]  = (vib[i] - stats.mean_vib) * inv_std_vib;
        norm_temp[i] = (temp[i] - stats.mean_temp) * inv_std_temp;
    }
}

void freeze() {
    frozen = true;
}

} // namespace normalizer
