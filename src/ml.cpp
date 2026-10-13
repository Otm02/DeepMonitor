#include "ml.h"
#include <cmath>
#include <cstddef>
#include <cstring>

#ifdef HOST_TEST
#include <random>
#else
#include <esp_random.h>
#include <cstdlib>
#endif

namespace ml {

namespace {

constexpr int F = config::AE_INPUT_DIM;
constexpr int H = config::AE_HIDDEN_DIM;
constexpr int L = config::AE_LATENT_DIM;

// Forward pass. Fills hidden/latent activations and the reconstruction.
void forward(const float* x, const Weights& w,
             float* h1, float* z, float* h2, float* xr) {
    for (int j = 0; j < H; ++j) {
        float s = w.b1[j];
        for (int i = 0; i < F; ++i) s += w.W1[j][i] * x[i];
        h1[j] = std::tanh(s);
    }
    for (int j = 0; j < L; ++j) {
        float s = w.b2[j];
        for (int i = 0; i < H; ++i) s += w.W2[j][i] * h1[i];
        z[j] = s; // linear latent
    }
    for (int j = 0; j < H; ++j) {
        float s = w.b3[j];
        for (int i = 0; i < L; ++i) s += w.W3[j][i] * z[i];
        h2[j] = std::tanh(s);
    }
    for (int j = 0; j < F; ++j) {
        float s = w.b4[j];
        for (int i = 0; i < H; ++i) s += w.W4[j][i] * h2[i];
        xr[j] = s; // linear output
    }
}

} // namespace

InferenceResult infer(const float input[config::AE_INPUT_DIM], const Weights& w, float threshold) {
    InferenceResult result;
    float h1[H], z[L], h2[H], xr[F];
    forward(input, w, h1, z, h2, xr);

    float mse = 0.0f;
    for (int i = 0; i < F; ++i) {
        float d = input[i] - xr[i];
        mse += d * d;
    }
    mse /= static_cast<float>(F);

    result.reconstruction_error = mse;
    result.is_anomaly = (mse > threshold);
    for (int j = 0; j < L; ++j) result.latent[j] = z[j];
    return result;
}

void update_weights(const float input[config::AE_INPUT_DIM], const InferenceResult& result, Weights& w) {
    // Anomalous cycle: skip learning, manage streak / LR reset.
    if (result.is_anomaly) {
        w.anomaly_streak++;
        if (w.anomaly_streak >= config::LR_RESET_STREAK) {
            w.learning_rate = config::LR_INITIAL;
            w.anomaly_streak = 0;
        }
        return;
    }

    w.anomaly_streak = 0;
    w.normal_cycle_count++;
    float lr = config::LR_INITIAL / (1.0f + config::LR_DECAY_RATE * static_cast<float>(w.normal_cycle_count));
    w.learning_rate = lr;

    // Recompute forward pass to obtain activations for backprop.
    float h1[H], z[L], h2[H], xr[F];
    forward(input, w, h1, z, h2, xr);

    // Output layer gradient: dL/dxr = (2/F)(xr - x).
    float go[F];
    const float scale = 2.0f / static_cast<float>(F);
    for (int i = 0; i < F; ++i) go[i] = scale * (xr[i] - input[i]);

    // Backprop into h2 (through W4), then through tanh.
    float dpre2[H];
    for (int j = 0; j < H; ++j) {
        float s = 0.0f;
        for (int i = 0; i < F; ++i) s += w.W4[i][j] * go[i];
        dpre2[j] = s * (1.0f - h2[j] * h2[j]);
    }
    // Backprop into z (through W3); latent is linear.
    float dz[L];
    for (int j = 0; j < L; ++j) {
        float s = 0.0f;
        for (int i = 0; i < H; ++i) s += w.W3[i][j] * dpre2[i];
        dz[j] = s;
    }
    // Backprop into h1 (through W2), then through tanh.
    float dpre1[H];
    for (int j = 0; j < H; ++j) {
        float s = 0.0f;
        for (int i = 0; i < L; ++i) s += w.W2[i][j] * dz[i];
        dpre1[j] = s * (1.0f - h1[j] * h1[j]);
    }

    // Apply SGD updates (W -= lr * grad).
    for (int i = 0; i < F; ++i) {
        for (int j = 0; j < H; ++j) w.W4[i][j] -= lr * go[i] * h2[j];
        w.b4[i] -= lr * go[i];
    }
    for (int i = 0; i < H; ++i) {
        for (int j = 0; j < L; ++j) w.W3[i][j] -= lr * dpre2[i] * z[j];
        w.b3[i] -= lr * dpre2[i];
    }
    for (int i = 0; i < L; ++i) {
        for (int j = 0; j < H; ++j) w.W2[i][j] -= lr * dz[i] * h1[j];
        w.b2[i] -= lr * dz[i];
    }
    for (int i = 0; i < H; ++i) {
        for (int j = 0; j < F; ++j) w.W1[i][j] -= lr * dpre1[i] * input[j];
        w.b1[i] -= lr * dpre1[i];
    }
}

void init_weights_random(Weights& w) {
    // Xavier-ish uniform init scaled per fan-in.
    auto rnd = [&]() -> float {
#ifdef HOST_TEST
        static std::mt19937 rng(42);
        static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
        return dist(rng);
#else
        return (static_cast<float>(esp_random()) / static_cast<float>(UINT32_MAX)) * 2.0f - 1.0f;
#endif
    };

    const float s1 = std::sqrt(1.0f / static_cast<float>(F));
    const float s2 = std::sqrt(1.0f / static_cast<float>(H));
    const float s3 = std::sqrt(1.0f / static_cast<float>(L));
    const float s4 = std::sqrt(1.0f / static_cast<float>(H));

    for (int j = 0; j < H; ++j) {
        for (int i = 0; i < F; ++i) w.W1[j][i] = rnd() * s1;
        w.b1[j] = 0.0f;
    }
    for (int j = 0; j < L; ++j) {
        for (int i = 0; i < H; ++i) w.W2[j][i] = rnd() * s2;
        w.b2[j] = 0.0f;
    }
    for (int j = 0; j < H; ++j) {
        for (int i = 0; i < L; ++i) w.W3[j][i] = rnd() * s3;
        w.b3[j] = 0.0f;
    }
    for (int j = 0; j < F; ++j) {
        for (int i = 0; i < H; ++i) w.W4[j][i] = rnd() * s4;
        w.b4[j] = 0.0f;
    }

    w.learning_rate = config::LR_INITIAL;
    w.normal_cycle_count = 0;
    w.anomaly_streak = 0;
}

float compute_weight_shift(const Weights& current, const Weights& saved) {
    float sum = 0.0f;
    const float* a = reinterpret_cast<const float*>(&current);
    const float* b = reinterpret_cast<const float*>(&saved);
    // Sum of squared differences over the weight/bias arrays only
    // (everything up to learning_rate, which is the first trailing scalar).
    const size_t n_floats = offsetof(Weights, learning_rate) / sizeof(float);
    for (size_t i = 0; i < n_floats; ++i) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return std::sqrt(sum);
}

void recon_push(ReconTracker& t, float err) {
    if (t.head >= config::RECON_HISTORY) t.head = 0;
    t.ring[t.head] = err;
    t.head = static_cast<uint8_t>((t.head + 1) % config::RECON_HISTORY);
    if (t.count < config::RECON_HISTORY) t.count++;
}

float adaptive_threshold(const ReconTracker& t, float k, float floor) {
    if (t.count < config::RECON_WARMUP) {
        // Not enough baseline yet: suppress anomalies during warmup.
        return 1e30f;
    }
    float mean = 0.0f;
    for (int i = 0; i < t.count; ++i) mean += t.ring[i];
    mean /= static_cast<float>(t.count);

    float var = 0.0f;
    for (int i = 0; i < t.count; ++i) {
        float d = t.ring[i] - mean;
        var += d * d;
    }
    var /= static_cast<float>(t.count);
    float thr = mean + k * std::sqrt(var);
    return (thr > floor) ? thr : floor;
}

} // namespace ml
