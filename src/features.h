#ifndef FEATURES_H
#define FEATURES_H

#include "config.h"

// Vibration feature extraction.
//
// Turns a raw per-cycle sample buffer (vibration magnitude + temperature) into
// a fixed-length feature vector capturing both time-domain shape and
// frequency-domain content. This is the input to the anomaly autoencoder.
//
// Feature layout (config::FEATURE_DIM total):
//   [0]  rms             RMS of the AC (mean-removed) vibration signal
//   [1]  peak            max |AC|
//   [2]  crest_factor    peak / rms
//   [3]  kurtosis        4th standardized moment (impulsiveness)
//   [4]  std             standard deviation
//   [5]  peak_to_peak    max(AC) - min(AC)
//   [6 .. 6+FREQ_BANDS-1] band_energy   log-compressed energy per frequency band
//   [..+0] spectral_centroid   normalized (0..1) energy-weighted mean frequency
//   [..+1] dominant_freq       normalized (0..1) bin of peak magnitude
//   [..+2] total_energy        log-compressed total spectral energy
//   [last] temp_mean           mean temperature over the window
namespace features {
    void extract(const float vib[config::SAMPLE_COUNT],
                 const float temp[config::SAMPLE_COUNT],
                 float out[config::FEATURE_DIM]);

    // Real-input radix-2 FFT magnitude spectrum.
    // `in` holds config::FFT_SIZE real samples; `mag_out` receives the
    // magnitude of bins [0, FFT_SIZE/2) (DC .. just below Nyquist).
    void fft_magnitude(const float in[config::FFT_SIZE],
                       float mag_out[config::FFT_SIZE / 2]);
}

#endif // FEATURES_H
