#ifndef CONNECTIVITY_H
#define CONNECTIVITY_H

#include <cstdint>
#include "normalizer.h"

namespace connectivity {
    struct AlertPayload {
        const char* device_id;
        uint32_t timestamp_ms;
        float reconstruction_error;
        float anomaly_threshold;
        float mean_vib;
        float mean_temp;
        float battery_voltage;
    };

    struct HeartbeatPayload {
        const char* device_id;
        uint32_t uptime_cycles;
        float battery_voltage;
        float anomaly_threshold;
        normalizer::Stats norm_stats;
        float weight_magnitude_summary;
        // Diagnostics for field troubleshooting without serial monitor
        uint32_t wifi_fail_count;
        uint32_t http_retry_count;
        float cycle_reconstruction_error;
        uint8_t cycle_is_anomaly;
    };

    bool connect_wifi(int timeout_ms = 10000);
    bool send_alert(const AlertPayload& payload, const char* webhook_url);
    bool send_heartbeat(const HeartbeatPayload& payload, const char* webhook_url);
    // POST pre-serialized JSON (used when draining flash buffer)
    bool post_json(const char* json, const char* webhook_url);
    const char* get_last_json();
    void disconnect_wifi();
    bool should_send_heartbeat(uint32_t duty_cycle_count, uint32_t last_heartbeat_cycle);

#ifdef HOST_TEST
    namespace mock {
        const char* get_last_json();
        void set_wifi_should_fail(bool fail);
        void set_http_should_fail(bool fail);
        void reset();
    }
#endif
}

#endif // CONNECTIVITY_H
