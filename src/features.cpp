#include "features.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace features {

namespace {

constexpr int N = config::FFT_SIZE;
constexpr int HALF = config::FFT_SIZE / 2;

// In-place iterative radix-2 Cooley-Tukey FFT on real input.
// re/im are length-N working buffers; im is zeroed by the caller.
void fft_inplace(float* re, float* im) {
    // Bit-reversal permutation.
    int j = 0;
    for (int i = 1; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }

    // Butterfly stages.
    for (int len = 2; len <= N; len <<= 1) {
        float ang = -2.0f * static_cast<float>(M_PI) / static_cast<float>(len);
        float wlen_re = std::cos(ang);
        float wlen_im = std::sin(ang);
        for (int i = 0; i < N; i += len) {
            float w_re = 1.0f;
            float w_im = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                int a = i + k;
                int b = i + k + len / 2;
                float u_re = re[a];
                float u_im = im[a];
                float v_re = re[b] * w_re - im[b] * w_im;
                float v_im = re[b] * w_im + im[b] * w_re;
                re[a] = u_re + v_re;
                im[a] = u_im + v_im;
                re[b] = u_re - v_re;
                im[b] = u_im - v_im;
                float nw_re = w_re * wlen_re - w_im * wlen_im;
                w_im = w_re * wlen_im + w_im * wlen_re;
                w_re = nw_re;
            }
        }
    }
}

} // namespace

void fft_magnitude(const float in[config::FFT_SIZE], float mag_out[config::FFT_SIZE / 2]) {
    static float re[N];
    static float im[N];
    for (int i = 0; i < N; ++i) {
        re[i] = in[i];
        im[i] = 0.0f;
    }
    fft_inplace(re, im);
    for (int i = 0; i < HALF; ++i) {
        mag_out[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]);
    }
}

void extract(const float vib[config::SAMPLE_COUNT],
             const float temp[config::SAMPLE_COUNT],
             float out[config::FEATURE_DIM]) {
    const int n = config::SAMPLE_COUNT;

    // ── Mean / temperature ───────────────────────────────────────────────────
    float sum_vib = 0.0f;
    float sum_temp = 0.0f;
    for (int i = 0; i < n; ++i) {
        sum_vib += vib[i];
        sum_temp += temp[i];
    }
    const float mean_vib = sum_vib / static_cast<float>(n);
    const float mean_temp = sum_temp / static_cast<float>(n);

    // ── Time-domain features on the AC (mean-removed) signal ──────────────────
    float sum_sq = 0.0f;   // for RMS / std
    float sum_4 = 0.0f;    // for kurtosis
    float peak = 0.0f;     // max |AC|
    float ac_min = 0.0f;
    float ac_max = 0.0f;
    bool first = true;
    for (int i = 0; i < n; ++i) {
        float ac = vib[i] - mean_vib;
        sum_sq += ac * ac;
        sum_4 += ac * ac * ac * ac;
        float a = std::fabs(ac);
        if (a > peak) peak = a;
        if (first) { ac_min = ac; ac_max = ac; first = false; }
        else { if (ac < ac_min) ac_min = ac; if (ac > ac_max) ac_max = ac; }
    }
    const float var = sum_sq / static_cast<float>(n);
    const float rms = std::sqrt(var);
    const float std_dev = rms; // AC RMS == std (mean already removed)
    const float crest = (rms > 1e-9f) ? (peak / rms) : 0.0f;
    const float m4 = sum_4 / static_cast<float>(n);
    const float kurtosis = (var > 1e-12f) ? (m4 / (var * var)) : 0.0f;
    const float p2p = ac_max - ac_min;

    // ── Frequency-domain features ─────────────────────────────────────────────
    static float fft_in[config::FFT_SIZE];
    static float mag[config::FFT_SIZE / 2];
    // Apply a Hann window to the AC signal over the first FFT_SIZE samples.
    for (int i = 0; i < config::FFT_SIZE; ++i) {
        float sample = (i < n) ? (vib[i] - mean_vib) : 0.0f;
        float w = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) *
                                          static_cast<float>(i) /
                                          static_cast<float>(config::FFT_SIZE - 1)));
        fft_in[i] = sample * w;
    }
    fft_magnitude(fft_in, mag);

    // Power per bin, skipping DC (bin 0).
    const int bins = config::FFT_SIZE / 2;
    float total_power = 0.0f;
    float centroid_num = 0.0f;
    float dom_mag = 0.0f;
    int dom_bin = 1;
    for (int b = 1; b < bins; ++b) {
        float p = mag[b] * mag[b];
        total_power += p;
        centroid_num += static_cast<float>(b) * p;
        if (mag[b] > dom_mag) { dom_mag = mag[b]; dom_bin = b; }
    }

    // Band energies: split bins [1, bins) into FREQ_BANDS contiguous groups.
    float band_energy[config::FREQ_BANDS];
    for (int k = 0; k < config::FREQ_BANDS; ++k) band_energy[k] = 0.0f;
    const int usable = bins - 1; // exclude DC
    for (int b = 1; b < bins; ++b) {
        int idx = ((b - 1) * config::FREQ_BANDS) / usable;
        if (idx >= config::FREQ_BANDS) idx = config::FREQ_BANDS - 1;
        band_energy[idx] += mag[b] * mag[b];
    }

    const float centroid = (total_power > 1e-12f)
        ? (centroid_num / total_power) / static_cast<float>(bins)
        : 0.0f;
    const float dominant = static_cast<float>(dom_bin) / static_cast<float>(bins);

    // ── Assemble feature vector ───────────────────────────────────────────────
    int o = 0;
    out[o++] = rms;
    out[o++] = peak;
    out[o++] = crest;
    out[o++] = kurtosis;
    out[o++] = std_dev;
    out[o++] = p2p;
    for (int k = 0; k < config::FREQ_BANDS; ++k) {
        out[o++] = std::log1p(band_energy[k]);
    }
    out[o++] = centroid;
    out[o++] = dominant;
    out[o++] = std::log1p(total_power);
    out[o++] = mean_temp;
    // o == config::FEATURE_DIM
}

} // namespace features
