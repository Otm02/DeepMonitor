#include "connectivity.h"
#include "config.h"
#include "rtc.h"
#include <cstdio>
#include <cstring>

static char s_json_buf[512];

#ifdef HOST_TEST

namespace {
    bool g_wifi_should_fail = false;
    bool g_http_should_fail = false;
}

namespace connectivity {

namespace mock {
    const char* get_last_json() { return s_json_buf; }
    void set_wifi_should_fail(bool fail) { g_wifi_should_fail = fail; }
    void set_http_should_fail(bool fail) { g_http_should_fail = fail; }
    void reset() {
        g_wifi_should_fail = false;
        g_http_should_fail = false;
        s_json_buf[0] = '\0';
    }
}

bool connect_wifi(int /*timeout_ms*/) {
    if (g_wifi_should_fail) return false;
    return true;
}

void disconnect_wifi() {}

const char* get_last_json() { return s_json_buf; }

bool post_json(const char* json, const char* /*webhook_url*/) {
    if (!json) return false;
    strncpy(s_json_buf, json, sizeof(s_json_buf) - 1);
    s_json_buf[sizeof(s_json_buf) - 1] = '\0';
    if (g_http_should_fail) return false;
    return true;
}

bool send_alert(const AlertPayload& payload, const char* /*webhook_url*/) {
    snprintf(s_json_buf, sizeof(s_json_buf),
        "{\"type\":\"anomaly\","
        "\"device_id\":\"%s\","
        "\"timestamp_ms\":%u,"
        "\"reconstruction_error\":%.4f,"
        "\"anomaly_threshold\":%.4f,"
        "\"mean_vibration\":%.3f,"
        "\"mean_temperature_delta\":%.3f,"
        "\"battery_voltage\":%.2f}",
        payload.device_id,
        (unsigned)payload.timestamp_ms,
        (double)payload.reconstruction_error,
        (double)payload.anomaly_threshold,
        (double)payload.mean_vib,
        (double)payload.mean_temp,
        (double)payload.battery_voltage);

    if (g_http_should_fail) return false;
    return true;
}

bool send_heartbeat(const HeartbeatPayload& payload, const char* /*webhook_url*/) {
    snprintf(s_json_buf, sizeof(s_json_buf),
        "{\"type\":\"heartbeat\","
        "\"device_id\":\"%s\","
        "\"uptime_cycles\":%u,"
        "\"battery_voltage\":%.2f,"
        "\"anomaly_threshold\":%.4f,"
        "\"norm_mean_vib\":%.2f,"
        "\"norm_var_vib\":%.2f,"
        "\"norm_mean_temp\":%.2f,"
        "\"norm_var_temp\":%.2f,"
        "\"weight_magnitude\":%.2f,"
        "\"wifi_fail_count\":%u,"
        "\"http_retry_count\":%u,"
        "\"cycle_reconstruction_error\":%.4f,"
        "\"cycle_is_anomaly\":%u}",
        payload.device_id,
        (unsigned)payload.uptime_cycles,
        (double)payload.battery_voltage,
        (double)payload.anomaly_threshold,
        (double)payload.norm_stats.mean_vib,
        (double)payload.norm_stats.var_vib,
        (double)payload.norm_stats.mean_temp,
        (double)payload.norm_stats.var_temp,
        (double)payload.weight_magnitude_summary,
        (unsigned)payload.wifi_fail_count,
        (unsigned)payload.http_retry_count,
        (double)payload.cycle_reconstruction_error,
        (unsigned)payload.cycle_is_anomaly);

    if (g_http_should_fail) return false;
    return true;
}

bool should_send_heartbeat(uint32_t duty_cycle_count, uint32_t last_heartbeat_cycle) {
    return (duty_cycle_count - last_heartbeat_cycle) >= static_cast<uint32_t>(config::HEARTBEAT_INTERVAL);
}

} // namespace connectivity

#else

#include <WiFi.h>
#include <HTTPClient.h>

extern rtc::RtcBlock rtc_block;

namespace {

bool has_api_key() {
    return config::SUPABASE_APIKEY_STR[0] != '\0';
}

void add_auth_headers(HTTPClient& http) {
    http.addHeader("Content-Type", "application/json");
    if (has_api_key()) {
        char auth_hdr[128];
        snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", config::SUPABASE_APIKEY_STR);
        http.addHeader("Authorization", auth_hdr);
        http.addHeader("apikey", config::SUPABASE_APIKEY_STR);
    }
}

bool do_post(const char* webhook_url, const char* json) {
    HTTPClient http;
    http.begin(webhook_url);
    add_auth_headers(http);
    int code = http.POST(json);
    http.end();
    return (code >= 200 && code < 300);
}

} // namespace

namespace connectivity {

bool connect_wifi(int timeout_ms) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config::WIFI_SSID_STR, config::WIFI_PASS_STR);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if ((int)(millis() - start) >= timeout_ms) {
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            rtc_block.wifi_fail_count++;
            return false;
        }
        delay(100);
    }
    return true;
}

void disconnect_wifi() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
}

const char* get_last_json() { return s_json_buf; }

bool post_json(const char* json, const char* webhook_url) {
    if (!json || !webhook_url) return false;
    if (!do_post(webhook_url, json)) {
        rtc_block.http_retry_count++;
        return false;
    }
    return true;
}

bool send_alert(const AlertPayload& payload, const char* webhook_url) {
    snprintf(s_json_buf, sizeof(s_json_buf),
        "{\"type\":\"anomaly\","
        "\"device_id\":\"%s\","
        "\"timestamp_ms\":%u,"
        "\"reconstruction_error\":%.4f,"
        "\"anomaly_threshold\":%.4f,"
        "\"mean_vibration\":%.3f,"
        "\"mean_temperature_delta\":%.3f,"
        "\"battery_voltage\":%.2f}",
        payload.device_id,
        (unsigned)payload.timestamp_ms,
        (double)payload.reconstruction_error,
        (double)payload.anomaly_threshold,
        (double)payload.mean_vib,
        (double)payload.mean_temp,
        (double)payload.battery_voltage);

    if (!do_post(webhook_url, s_json_buf)) {
        rtc_block.http_retry_count++;
        return false;
    }
    return true;
}

bool send_heartbeat(const HeartbeatPayload& payload, const char* webhook_url) {
    snprintf(s_json_buf, sizeof(s_json_buf),
        "{\"type\":\"heartbeat\","
        "\"device_id\":\"%s\","
        "\"uptime_cycles\":%u,"
        "\"battery_voltage\":%.2f,"
        "\"anomaly_threshold\":%.4f,"
        "\"norm_mean_vib\":%.2f,"
        "\"norm_var_vib\":%.2f,"
        "\"norm_mean_temp\":%.2f,"
        "\"norm_var_temp\":%.2f,"
        "\"weight_magnitude\":%.2f,"
        "\"wifi_fail_count\":%u,"
        "\"http_retry_count\":%u,"
        "\"cycle_reconstruction_error\":%.4f,"
        "\"cycle_is_anomaly\":%u}",
        payload.device_id,
        (unsigned)payload.uptime_cycles,
        (double)payload.battery_voltage,
        (double)payload.anomaly_threshold,
        (double)payload.norm_stats.mean_vib,
        (double)payload.norm_stats.var_vib,
        (double)payload.norm_stats.mean_temp,
        (double)payload.norm_stats.var_temp,
        (double)payload.weight_magnitude_summary,
        (unsigned)payload.wifi_fail_count,
        (unsigned)payload.http_retry_count,
        (double)payload.cycle_reconstruction_error,
        (unsigned)payload.cycle_is_anomaly);

    if (!do_post(webhook_url, s_json_buf)) {
        rtc_block.http_retry_count++;
        return false;
    }
    return true;
}

bool should_send_heartbeat(uint32_t duty_cycle_count, uint32_t last_heartbeat_cycle) {
    return (duty_cycle_count - last_heartbeat_cycle) >= static_cast<uint32_t>(config::HEARTBEAT_INTERVAL);
}

} // namespace connectivity

#endif // HOST_TEST
