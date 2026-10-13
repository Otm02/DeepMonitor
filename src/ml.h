#ifndef ML_H
#define ML_H

#include <cstdint>
#include "config.h"

// Compact nonlinear autoencoder for unsupervised anomaly detection.
//
//   input(F) --W1,b1--> tanh --W2,b2--> latent(L) --W3,b3--> tanh --W4,b4--> output(F)
//
// Trained online with plain SGD on reconstruction MSE, using only normal
// cycles (anomalous cycles skip the weight update). The reconstruction error
// is compared against an adaptive threshold (see ReconTracker) to flag
// anomalies, so the device self-calibrates per installation.
namespace ml {
    struct Weights {
        float W1[config::AE_HIDDEN_DIM][config::AE_INPUT_DIM];
        float b1[config::AE_HIDDEN_DIM];
        float W2[config::AE_LATENT_DIM][config::AE_HIDDEN_DIM];
        float b2[config::AE_LATENT_DIM];
        float W3[config::AE_HIDDEN_DIM][config::AE_LATENT_DIM];
        float b3[config::AE_HIDDEN_DIM];
        float W4[config::AE_INPUT_DIM][config::AE_HIDDEN_DIM];
        float b4[config::AE_INPUT_DIM];
        float learning_rate;
        int normal_cycle_count;
        int anomaly_streak;
    };

    struct InferenceResult {
        float reconstruction_error;
        bool is_anomaly;
        float latent[config::AE_LATENT_DIM];
    };

    // Rolling history of recent (normal) reconstruction errors, used to derive
    // the adaptive anomaly threshold.
    struct ReconTracker {
        float ring[config::RECON_HISTORY];
        uint8_t head;
        uint8_t count;
    };

    InferenceResult infer(const float input[config::AE_INPUT_DIM], const Weights& w, float threshold);
    void update_weights(const float input[config::AE_INPUT_DIM], const InferenceResult& result, Weights& w);
    void init_weights_random(Weights& w);
    float compute_weight_shift(const Weights& current, const Weights& saved);

    // Adaptive threshold helpers.
    void recon_push(ReconTracker& t, float err);
    float adaptive_threshold(const ReconTracker& t, float k, float floor);
}

#endif // ML_H
