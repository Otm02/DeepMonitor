#ifndef CONFIG_H
#define CONFIG_H

#include <cstdint>

// Optional secrets (gitignored src/secrets.h or PlatformIO -D flags)
#if __has_include("secrets.h")
    #include "secrets.h"
#endif

#ifndef WIFI_SSID
    #define WIFI_SSID "..."
#endif
#ifndef WIFI_PASS
    #define WIFI_PASS "..."
#endif
#ifndef WEBHOOK_URL
    #define WEBHOOK_URL "http://192.168.1.100:8000/ingest"
#endif
#ifndef CONFIG_URL
    // Device pulls runtime config from here (control plane). Empty disables pull.
    #define CONFIG_URL ""
#endif
#ifndef SUPABASE_APIKEY
    #define SUPABASE_APIKEY ""
#endif
#ifndef DEVICE_ID
    #define DEVICE_ID "M5-XXXX"
#endif

namespace config {
    // ── Sampling ────────────────────────────────────────────────────────────
    // 256 samples @ 200 Hz ≈ 1.28 s active window, Nyquist 100 Hz.
    static const int   SAMPLE_COUNT          = 256;
    static const int   SAMPLE_RATE_HZ        = 200;

    // ── Feature extraction ──────────────────────────────────────────────────
    static const int   FFT_SIZE              = 256;   // must be a power of two, <= SAMPLE_COUNT
    static const int   FREQ_BANDS            = 14;    // spectral band-energy features
    // Feature layout (see features.cpp): 6 time-domain + FREQ_BANDS + 3 spectral + 1 temp
    static const int   FEATURE_DIM           = 24;

    // ── Autoencoder (compact nonlinear MLP) ──────────────────────────────────
    static const int   AE_INPUT_DIM          = FEATURE_DIM; // 24
    static const int   AE_HIDDEN_DIM         = 16;
    static const int   AE_LATENT_DIM         = 6;

    // ── Adaptive anomaly threshold ────────────────────────────────────────────
    // threshold = max(ANOMALY_THRESHOLD, mean_recon + ANOMALY_K * std_recon)
    static const int   RECON_HISTORY         = 32;    // ring buffer of normal recon errors
    static const int   RECON_WARMUP          = 8;     // cycles before adaptive threshold engages
    static const float ANOMALY_K             = 3.0f;  // sensitivity (sigmas above mean)
    static const float ANOMALY_THRESHOLD     = 0.10f; // floor / bootstrap threshold

    // ── Online learning ───────────────────────────────────────────────────────
    static const float SMOOTHING_ALPHA       = 0.05f; // feature EMA factor
    static const float LR_INITIAL            = 0.01f;
    static const float LR_DECAY_RATE         = 0.001f;
    static const int   LR_RESET_STREAK       = 10;

    // ── Duty cycle / persistence (compile-time defaults; runtime-overridable) ──
    // Battery-optimal profile (see tools/battery_sim.py --recommend): 15-min
    // sampling + ~1.3 s active window yields ~3.8 years on a 3000 mAh 18650
    // while keeping detection latency at 15 min. Tune live from the dashboard.
    static const uint32_t SLEEP_DURATION_US  = 900000000u; // 15 minutes
    static const int   NVS_SAVE_INTERVAL     = 96;         // 24h at 15-min cycles
    static const float WEIGHT_SHIFT_THRESHOLD = 0.5f;
    static const uint32_t HEARTBEAT_INTERVAL = 96;         // ~24h at 15-min cycles
    static const int   WIFI_TIMEOUT_MS       = 10000;

    // ── Network credentials and endpoints (from secrets.h or build flags) ──────
    static const char* const WIFI_SSID_STR       = WIFI_SSID;
    static const char* const WIFI_PASS_STR       = WIFI_PASS;
    static const char* const WEBHOOK_URL_STR     = WEBHOOK_URL;
    static const char* const CONFIG_URL_STR      = CONFIG_URL;
    static const char* const SUPABASE_APIKEY_STR = SUPABASE_APIKEY;
    static const char* const DEVICE_ID_STR       = DEVICE_ID;

    // ── Runtime device configuration (mutable via the dashboard control plane) ─
    // Persisted in RTC; pulled from CONFIG_URL during Wi-Fi sessions.
    struct DeviceConfig {
        uint32_t sleep_duration_us;
        uint32_t heartbeat_interval;
        uint32_t sample_rate_hz;
        float    anomaly_k;
        uint32_t config_version;
    };

    inline DeviceConfig default_config() {
        DeviceConfig c;
        c.sleep_duration_us  = SLEEP_DURATION_US;
        c.heartbeat_interval = HEARTBEAT_INTERVAL;
        c.sample_rate_hz     = static_cast<uint32_t>(SAMPLE_RATE_HZ);
        c.anomaly_k          = ANOMALY_K;
        c.config_version     = 0;
        return c;
    }
}

#endif // CONFIG_H
