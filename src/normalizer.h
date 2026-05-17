#ifndef NORMALIZER_H
#define NORMALIZER_H

#include "config.h"

namespace normalizer {
    struct Stats {
        float mean_vib;
        float var_vib;
        float mean_temp;
        float var_temp;
    };

    void update(const float vib[config::SAMPLE_COUNT], const float temp[config::SAMPLE_COUNT], Stats& stats, float alpha);
    void normalize(const float vib[config::SAMPLE_COUNT], const float temp[config::SAMPLE_COUNT],
                   const Stats& stats,
                   float norm_vib[config::SAMPLE_COUNT], float norm_temp[config::SAMPLE_COUNT]);
    void freeze();
}

#endif // NORMALIZER_H
