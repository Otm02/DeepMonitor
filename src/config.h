#ifndef CONFIG_H
#define CONFIG_H

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
#ifndef SUPABASE_APIKEY
    #define SUPABASE_APIKEY ""
#endif
#ifndef DEVICE_ID
    #define DEVICE_ID "M5-XXXX"
#endif

namespace config {
    // EMA smoothing factor (Requirement 2.2)
    static const float SMOOTHING_ALPHA       = 0.1f;

    // Reconstruction error threshold (Requirement 4.6)
    static const float ANOMALY_THRESHOLD     = 0.03f;

    // Learning rate parameters (Requirement 4.6)
    static const float LR_INITIAL            = 0.01f;
    static const float LR_DECAY_RATE         = 0.001f;
    static const int   LR_RESET_STREAK       = 10;

    // Deep sleep interval: 5 minutes in microseconds (Requirement 5.1)
    static const int   SLEEP_DURATION_US     = 300000000;

    // NVS save every 288 cycles = 24h at 5-min intervals (Requirement 7.1)
    static const int   NVS_SAVE_INTERVAL     = 288;

    // NVS weight-shift save trigger (Requirement 7.2)
    static const float WEIGHT_SHIFT_THRESHOLD = 0.5f;

    // Heartbeat every 144 cycles = 12h at 5-min intervals (Requirement 9.1)
    static const int   HEARTBEAT_INTERVAL    = 144;

    // Wi-Fi connection timeout in milliseconds
    static const int   WIFI_TIMEOUT_MS       = 10000;

    // Sampling parameters: 100Hz x 5s = 500 samples
    static const int   SAMPLE_COUNT          = 500;
    static const int   SAMPLE_RATE_HZ        = 100;

    // ML model dimensions
    static const int   WINDOW_SIZE           = 16;
    static const int   INPUT_DIM             = 32;  // WINDOW_SIZE * 2
    static const int   LATENT_DIM            = 4;

    // Network credentials and endpoint (from secrets.h or build flags)
    static const char* const WIFI_SSID_STR       = WIFI_SSID;
    static const char* const WIFI_PASS_STR       = WIFI_PASS;
    static const char* const WEBHOOK_URL_STR     = WEBHOOK_URL;
    static const char* const SUPABASE_APIKEY_STR = SUPABASE_APIKEY;
    static const char* const DEVICE_ID_STR       = DEVICE_ID;
}

#endif // CONFIG_H
