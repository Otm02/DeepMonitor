#ifndef ML_H
#define ML_H

#include "config.h"

namespace ml {
    struct Weights {
        float encoder[config::LATENT_DIM][config::INPUT_DIM];
        float decoder[config::INPUT_DIM][config::LATENT_DIM];
        float learning_rate;
        int normal_cycle_count;
        int anomaly_streak;
    };

    struct InferenceResult {
        float reconstruction_error;
        bool is_anomaly;
        float latent[config::LATENT_DIM];
    };

    InferenceResult infer(const float input[config::INPUT_DIM], const Weights& w, float threshold);
    void update_weights(const float input[config::INPUT_DIM], const InferenceResult& result, Weights& w);
    void init_weights_random(Weights& w);
    float compute_weight_shift(const Weights& current, const Weights& saved);
}

#endif // ML_H
