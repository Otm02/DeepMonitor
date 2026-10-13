#include "connectivity.h"
#include "config.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

static char s_json_buf[1600];

namespace connectivity {

// ── Shared JSON helpers (compiled for host and device) ──────────────────────

bool json_get_number(const char* json, const char* key, double& out) {
    if (!json || !key) return false;
    char needle[64];
    std::snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* p = std::strstr(json, needle);
    if (!p) return false;
    p += std::strlen(needle);
    while (*p == ' ' || *p == '\t' || *p == ':') ++p;
    char* end = nullptr;
    double v = std::strtod(p, &end);
    if (end == p) return false;
    out = v;
    return true;
}

bool parse_config_json(const char* json, config::DeviceConfig& cfg) {
    double ver;
    if (!json_get_number(json, "config_version", ver)) return false;
    if (static_cast<uint32_t>(ver) <= cfg.config_version) return false;
    double v;
    if (json_get_number(json, "sleep_duration_us", v)) cfg.sleep_duration_us = static_cast<uint32_t>(v);
    if (json_get_number(json, "heartbeat_interval", v)) cfg.heartbeat_interval = static_cast<uint32_t>(v);
    if (json_get_number(json, "sample_rate_hz", v)) cfg.sample_rate_hz = static_cast<uint32_t>(v);
    if (json_get_number(json, "anomaly_k", v)) cfg.anomaly_k = static_cast<float>(v);
    cfg.config_version = static_cast<uint32_t>(ver);
    return true;
}

bool should_send_heartbeat(uint32_t duty_cycle_count, uint32_t last_heartbeat_cycle,
                           uint32_t heartbeat_interval) {
    return (duty_cycle_count - last_heartbeat_cycle) >= heartbeat_interval;
}

namespace {

void append_features(const float* feats, int n) {
    if (!feats || n <= 0) return;
    size_t len = std::strlen(s_json_buf);
    int w = std::snprintf(s_json_buf + len, sizeof(s_json_buf) - len, ",\"features\":[");
    if (w < 0) return;
    len = std::strlen(s_json_buf);
    for (int i = 0; i < n && len < sizeof(s_json_buf) - 20; ++i) {
        w = std::snprintf(s_json_buf + len, sizeof(s_json_buf) - len,
                          "%s%.5g", (i == 0 ? "" : ","), static_cast<double>(feats[i]));
        if (w < 0) return;
        len = std::strlen(s_json_buf);
    }
    std::snprintf(s_json_buf + len, sizeof(s_json_buf) - len, "]");
}

void close_json() {
    size_t len = std::strlen(s_json_buf);
    std::snprintf(s_json_buf + len, sizeof(s_json_buf) - len, "}");
}

void build_alert_json(const AlertPayload& p) {
    std::snprintf(s_json_buf, sizeof(s_json_buf),
        "{\"type\":\"anomaly\","
        "\"device_id\":\"%s\","
        "\"timestamp_ms\":%u,"
        "\"reconstruction_error\":%.5f,"
        "\"anomaly_threshold\":%.5f,"
        "\"mean_vibration\":%.4f,"
        "\"mean_temperature_delta\":%.4f,"
        "\"battery_voltage\":%.2f",
        p.device_id,
        (unsigned)p.timestamp_ms,
        (double)p.reconstruction_error,
        (double)p.anomaly_threshold,
        (double)p.mean_vib,
        (double)p.mean_temp,
        (double)p.battery_voltage);
    append_features(p.features, p.feature_count);
    close_json();
}

void build_heartbeat_json(const HeartbeatPayload& p) {
    std::snprintf(s_json_buf, sizeof(s_json_buf),
        "{\"type\":\"heartbeat\","
        "\"device_id\":\"%s\","
        "\"uptime_cycles\":%u,"
        "\"battery_voltage\":%.2f,"
        "\"anomaly_threshold\":%.5f,"
        "\"reconstruction_error\":%.5f,"
        "\"is_anomaly\":%u,"
        "\"weight_magnitude\":%.4f,"
        "\"wifi_fail_count\":%u,"
        "\"http_retry_count\":%u,"
        "\"config_version\":%u,"
        "\"sleep_duration_us\":%u,"
        "\"heartbeat_interval\":%u,"
        "\"sample_rate_hz\":%u",
        p.device_id,
        (unsigned)p.uptime_cycles,
        (double)p.battery_voltage,
        (double)p.anomaly_threshold,
        (double)p.reconstruction_error,
        (unsigned)p.is_anomaly,
        (double)p.weight_magnitude_summary,
        (unsigned)p.wifi_fail_count,
        (unsigned)p.http_retry_count,
        (unsigned)p.config_version,
        (unsigned)p.sleep_duration_us,
        (unsigned)p.heartbeat_interval,
        (unsigned)p.sample_rate_hz);
    append_features(p.features, p.feature_count);
    close_json();
}

} // namespace

const char* get_last_json() { return s_json_buf; }

} // namespace connectivity

#ifdef HOST_TEST

namespace {
    bool g_wifi_should_fail = false;
    bool g_http_should_fail = false;
    char g_config_json[512] = {0};
}

namespace connectivity {

namespace mock {
    const char* get_last_json() { return s_json_buf; }
    void set_wifi_should_fail(bool fail) { g_wifi_should_fail = fail; }
    void set_http_should_fail(bool fail) { g_http_should_fail = fail; }
    void set_config_json(const char* json) {
        if (!json) { g_config_json[0] = '\0'; return; }
        std::strncpy(g_config_json, json, sizeof(g_config_json) - 1);
        g_config_json[sizeof(g_config_json) - 1] = '\0';
    }
    void reset() {
        g_wifi_should_fail = false;
        g_http_should_fail = false;
        g_config_json[0] = '\0';
        s_json_buf[0] = '\0';
    }
}

bool connect_wifi(int /*timeout_ms*/) {
    return !g_wifi_should_fail;
}

void disconnect_wifi() {}

bool post_json(const char* json, const char* /*webhook_url*/) {
    if (!json) return false;
    std::strncpy(s_json_buf, json, sizeof(s_json_buf) - 1);
    s_json_buf[sizeof(s_json_buf) - 1] = '\0';
    return !g_http_should_fail;
}

bool send_alert(const AlertPayload& payload, const char* /*webhook_url*/) {
    build_alert_json(payload);
    return !g_http_should_fail;
}

bool send_heartbeat(const HeartbeatPayload& payload, const char* /*webhook_url*/) {
    build_heartbeat_json(payload);
    return !g_http_should_fail;
}

bool fetch_config(const char* /*config_url*/, config::DeviceConfig& cfg) {
    if (g_config_json[0] == '\0') return false;
    return parse_config_json(g_config_json, cfg);
}

} // namespace connectivity

#else

#include <WiFi.h>
#include <HTTPClient.h>
#include "rtc.h"

extern rtc::RtcBlock rtc_block;

namespace {

bool has_api_key() {
    return config::SUPABASE_APIKEY_STR[0] != '\0';
}

void add_auth_headers(HTTPClient& http) {
    http.addHeader("Content-Type", "application/json");
    if (has_api_key()) {
        char auth_hdr[128];
        std::snprintf(auth_hdr, sizeof(auth_hdr), "Bearer %s", config::SUPABASE_APIKEY_STR);
        http.addHeader("Authorization", auth_hdr);
        http.addHeader("apikey", config::SUPABASE_APIKEY_STR);
    }
}

bool do_post(const char* webhook_url, const char* json) {
    HTTPClient http;
    http.begin(webhook_url);
    add_auth_headers(http);
    // Arduino HTTPClient::POST takes a non-const pointer but does not mutate it.
    int code = http.POST(const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(json)),
                         static_cast<size_t>(std::strlen(json)));
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

bool post_json(const char* json, const char* webhook_url) {
    if (!json || !webhook_url) return false;
    if (!do_post(webhook_url, json)) {
        rtc_block.http_retry_count++;
        return false;
    }
    return true;
}

bool send_alert(const AlertPayload& payload, const char* webhook_url) {
    build_alert_json(payload);
    if (!do_post(webhook_url, s_json_buf)) {
        rtc_block.http_retry_count++;
        return false;
    }
    return true;
}

bool send_heartbeat(const HeartbeatPayload& payload, const char* webhook_url) {
    build_heartbeat_json(payload);
    if (!do_post(webhook_url, s_json_buf)) {
        rtc_block.http_retry_count++;
        return false;
    }
    return true;
}

bool fetch_config(const char* config_url, config::DeviceConfig& cfg) {
    if (!config_url || config_url[0] == '\0') return false;
    HTTPClient http;
    http.begin(config_url);
    add_auth_headers(http);
    int code = http.GET();
    bool applied = false;
    if (code >= 200 && code < 300) {
        String body = http.getString();
        applied = parse_config_json(body.c_str(), cfg);
    }
    http.end();
    return applied;
}

} // namespace connectivity

#endif // HOST_TEST
