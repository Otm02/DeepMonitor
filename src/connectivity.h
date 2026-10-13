#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include <cstdint>
#include "config.h"

namespace connectivity {
    struct AlertPayload {
        const char* device_id;
        uint32_t timestamp_ms;
        float reconstruction_error;
        float anomaly_threshold;
        float mean_vib;
        float mean_temp;
        float battery_voltage;
        const float* features;   // optional feature vector (may be null)
        int feature_count;
    };

    struct HeartbeatPayload {
        const char* device_id;
        uint32_t uptime_cycles;
        float battery_voltage;
        float anomaly_threshold;
        float reconstruction_error;   // most recent cycle
        uint8_t is_anomaly;
        float weight_magnitude_summary;
        // Diagnostics for field troubleshooting without serial monitor
        uint32_t wifi_fail_count;
        uint32_t http_retry_count;
        // Applied runtime configuration (so the dashboard can confirm pushes)
        uint32_t config_version;
        uint32_t sleep_duration_us;
        uint32_t heartbeat_interval;
        uint32_t sample_rate_hz;
        const float* features;   // optional feature vector (may be null)
        int feature_count;
    };

    bool connect_wifi(int timeout_ms = 10000);
    bool send_alert(const AlertPayload& payload, const char* webhook_url);
    bool send_heartbeat(const HeartbeatPayload& payload, const char* webhook_url);
    // POST pre-serialized JSON (used when draining flash buffer)
    bool post_json(const char* json, const char* webhook_url);
    const char* get_last_json();
    void disconnect_wifi();
    bool should_send_heartbeat(uint32_t duty_cycle_count, uint32_t last_heartbeat_cycle,
                               uint32_t heartbeat_interval);

    // Control plane: pull runtime config (GET config_url). Returns true and
    // mutates `cfg` only when a newer config_version is received.
    bool fetch_config(const char* config_url, config::DeviceConfig& cfg);

    // Shared JSON helpers (host + device). Extract a numeric value for "key".
    bool json_get_number(const char* json, const char* key, double& out);
    // Apply a config JSON document to `cfg` if it carries a newer version.
    bool parse_config_json(const char* json, config::DeviceConfig& cfg);

#ifdef HOST_TEST
    namespace mock {
        const char* get_last_json();
        void set_wifi_should_fail(bool fail);
        void set_http_should_fail(bool fail);
        void set_config_json(const char* json);
        void reset();
    }
#endif
}

#endif // CONNECTIVITY_H
