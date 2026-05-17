#include "ml.h"
#include <cmath>
#include <cstring>

#ifdef HOST_TEST
#include <random>
#else
#include <esp_random.h>
#include <cstdlib>
#endif

namespace ml {

InferenceResult infer(const float input[config::INPUT_DIM], const Weights& w, float threshold) {
    InferenceResult result;

    // Encoder: latent[j] = Σ_i encoder[j][i] * input[i]
    static float latent[config::LATENT_DIM];
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        float sum = 0.0f;
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            sum += w.encoder[j][i] * input[i];
        }
        latent[j] = sum;
    }

    // Decoder: reconstructed[i] = Σ_j decoder[i][j] * latent[j]
    static float reconstructed[config::INPUT_DIM];
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        float sum = 0.0f;
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            sum += w.decoder[i][j] * latent[j];
        }
        reconstructed[i] = sum;
    }

    // MSE: (1/INPUT_DIM) * Σ_i (input[i] - reconstructed[i])²
    float mse = 0.0f;
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        float diff = input[i] - reconstructed[i];
        mse += diff * diff;
    }
    mse /= static_cast<float>(config::INPUT_DIM);

    result.reconstruction_error = mse;
    result.is_anomaly = (mse > threshold);

    // Copy latent to result
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        result.latent[j] = latent[j];
    }

    return result;
}

void update_weights(const float input[config::INPUT_DIM], const InferenceResult& result, Weights& w) {
    // If anomaly: skip weight update, manage streak/reset
    if (result.is_anomaly) {
        w.anomaly_streak++;
        if (w.anomaly_streak >= config::LR_RESET_STREAK) {
            w.learning_rate = config::LR_INITIAL;
            w.anomaly_streak = 0;
        }
        return;
    }

    // Normal cycle
    w.anomaly_streak = 0;
    w.normal_cycle_count++;

    // Decayed learning rate
    float lr = config::LR_INITIAL / (1.0f + config::LR_DECAY_RATE * static_cast<float>(w.normal_cycle_count));
    w.learning_rate = lr;

    // Sanger's Rule update for encoder weights
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            float lower_sum = 0.0f;
            for (int k = 0; k <= j; ++k) {
                lower_sum += result.latent[k] * w.encoder[k][i];
            }
            float dw = lr * result.latent[j] * (input[i] - lower_sum);
            w.encoder[j][i] += dw;
        }
    }

    // Decoder = transpose of encoder (tied weights)
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            w.decoder[i][j] = w.encoder[j][i];
        }
    }
}

void init_weights_random(Weights& w) {
#ifdef HOST_TEST
    std::mt19937 rng(42);  // Fixed seed for reproducibility in tests
    std::uniform_real_distribution<float> dist(-0.1f, 0.1f);
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            w.encoder[j][i] = dist(rng);
        }
    }
#else
    // Seed from ESP32 hardware RNG
    srand(static_cast<unsigned int>(esp_random()));
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            // Map rand() to [-0.1, 0.1]
            w.encoder[j][i] = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 0.2f - 0.1f;
        }
    }
#endif

    // Decoder = transpose of encoder
    for (int i = 0; i < config::INPUT_DIM; ++i) {
        for (int j = 0; j < config::LATENT_DIM; ++j) {
            w.decoder[i][j] = w.encoder[j][i];
        }
    }

    w.learning_rate = config::LR_INITIAL;
    w.normal_cycle_count = 0;
    w.anomaly_streak = 0;
}

float compute_weight_shift(const Weights& current, const Weights& saved) {
    float sum = 0.0f;
    for (int j = 0; j < config::LATENT_DIM; ++j) {
        for (int i = 0; i < config::INPUT_DIM; ++i) {
            float diff = current.encoder[j][i] - saved.encoder[j][i];
            sum += diff * diff;
        }
    }
    return std::sqrt(sum);
}

} // namespace ml
