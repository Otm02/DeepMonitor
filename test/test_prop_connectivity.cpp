#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cstdio>
#include <cstring>
#include "connectivity.h"
#include "config.h"

/**
 * **Validates: Requirements 8.2**
 * Alert JSON contains all required keys.
 */
RC_GTEST_PROP(AlertPayload, ContainsAllRequiredFields, ()) {
    connectivity::mock::reset();

    connectivity::AlertPayload payload;
    payload.device_id = config::DEVICE_ID_STR;
    payload.timestamp_ms = static_cast<uint32_t>(*rc::gen::inRange(0, 1000000));
    payload.reconstruction_error = static_cast<float>(*rc::gen::inRange(0, 10000)) / 1000.0f;
    payload.anomaly_threshold = static_cast<float>(*rc::gen::inRange(1, 1000)) / 1000.0f;
    payload.mean_vib = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    payload.mean_temp = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    payload.battery_voltage = static_cast<float>(*rc::gen::inRange(300, 420)) / 100.0f;
    payload.features = nullptr;
    payload.feature_count = 0;

    connectivity::send_alert(payload, "http://example.com/webhook");

    const char* json = connectivity::mock::get_last_json();
    RC_ASSERT(std::strlen(json) > 0);
    RC_ASSERT(std::strstr(json, "\"type\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"device_id\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"timestamp_ms\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"reconstruction_error\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"anomaly_threshold\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"mean_vibration\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"mean_temperature_delta\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"battery_voltage\"") != nullptr);
}

/**
 * Alert with a feature vector includes a "features" array.
 */
TEST(AlertPayload, IncludesFeaturesArrayWhenProvided) {
    connectivity::mock::reset();
    float feats[config::FEATURE_DIM];
    for (int i = 0; i < config::FEATURE_DIM; ++i) feats[i] = 0.1f * static_cast<float>(i);

    connectivity::AlertPayload p{};
    p.device_id = config::DEVICE_ID_STR;
    p.features = feats;
    p.feature_count = config::FEATURE_DIM;

    connectivity::send_alert(p, "http://example.com/webhook");
    const char* json = connectivity::mock::get_last_json();
    EXPECT_NE(std::strstr(json, "\"features\""), nullptr);
}

/**
 * **Validates: Requirements 8.4**
 * send_alert returns false when the HTTP layer fails but still serializes.
 */
RC_GTEST_PROP(HTTPRetry, SendAlertReturnsFalseOnHTTPFailure, ()) {
    connectivity::mock::reset();
    connectivity::mock::set_http_should_fail(true);

    connectivity::AlertPayload payload{};
    payload.device_id = config::DEVICE_ID_STR;
    payload.timestamp_ms = static_cast<uint32_t>(*rc::gen::inRange(0, 1000000));
    payload.reconstruction_error = static_cast<float>(*rc::gen::inRange(0, 10000)) / 1000.0f;
    payload.battery_voltage = static_cast<float>(*rc::gen::inRange(300, 420)) / 100.0f;

    bool result = connectivity::send_alert(payload, "http://example.com/webhook");
    RC_ASSERT(!result);
    RC_ASSERT(std::strlen(connectivity::mock::get_last_json()) > 0);
}

/**
 * **Validates: Requirements 9.1**
 * Heartbeat triggers iff (count - last) >= interval.
 */
RC_GTEST_PROP(HeartbeatInterval, TriggerMatchesIntervalCondition, ()) {
    const auto last_heartbeat_cycle = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    const auto offset = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    const uint32_t duty_cycle_count = last_heartbeat_cycle + offset;
    const auto interval = static_cast<uint32_t>(*rc::gen::inRange(1, 1000));

    const bool result = connectivity::should_send_heartbeat(duty_cycle_count, last_heartbeat_cycle, interval);
    const bool expected = (duty_cycle_count - last_heartbeat_cycle) >= interval;
    RC_ASSERT(result == expected);
}

/**
 * **Validates: Requirements 9.2**
 * Heartbeat JSON contains all required keys.
 */
RC_GTEST_PROP(HeartbeatPayload, ContainsAllRequiredFields, ()) {
    connectivity::mock::reset();

    connectivity::HeartbeatPayload payload{};
    payload.device_id = config::DEVICE_ID_STR;
    payload.uptime_cycles = static_cast<uint32_t>(*rc::gen::inRange(0, 1000000));
    payload.battery_voltage = static_cast<float>(*rc::gen::inRange(300, 420)) / 100.0f;
    payload.anomaly_threshold = static_cast<float>(*rc::gen::inRange(1, 1000)) / 1000.0f;
    payload.reconstruction_error = static_cast<float>(*rc::gen::inRange(0, 10000)) / 1000.0f;
    payload.is_anomaly = static_cast<uint8_t>(*rc::gen::inRange(0, 2));
    payload.weight_magnitude_summary = static_cast<float>(*rc::gen::inRange(0, 100000)) / 100.0f;
    payload.wifi_fail_count = static_cast<uint32_t>(*rc::gen::inRange(0, 100));
    payload.http_retry_count = static_cast<uint32_t>(*rc::gen::inRange(0, 100));
    payload.config_version = static_cast<uint32_t>(*rc::gen::inRange(0, 100));
    payload.sleep_duration_us = 300000000u;
    payload.heartbeat_interval = 144u;
    payload.sample_rate_hz = 200u;
    payload.features = nullptr;
    payload.feature_count = 0;

    connectivity::send_heartbeat(payload, "http://example.com/webhook");

    const char* json = connectivity::mock::get_last_json();
    RC_ASSERT(std::strlen(json) > 0);
    RC_ASSERT(std::strstr(json, "\"type\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"device_id\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"uptime_cycles\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"battery_voltage\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"anomaly_threshold\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"reconstruction_error\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"is_anomaly\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"weight_magnitude\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"config_version\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"sleep_duration_us\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"heartbeat_interval\"") != nullptr);
}

/**
 * Control plane: json_get_number extracts numeric values.
 */
TEST(ConfigParse, JsonGetNumberExtractsValues) {
    const char* json = "{\"config_version\":7,\"sleep_duration_us\":600000000,\"anomaly_k\":2.5}";
    double v = 0.0;
    EXPECT_TRUE(connectivity::json_get_number(json, "config_version", v));
    EXPECT_DOUBLE_EQ(v, 7.0);
    EXPECT_TRUE(connectivity::json_get_number(json, "sleep_duration_us", v));
    EXPECT_DOUBLE_EQ(v, 600000000.0);
    EXPECT_TRUE(connectivity::json_get_number(json, "anomaly_k", v));
    EXPECT_NEAR(v, 2.5, 1e-9);
    EXPECT_FALSE(connectivity::json_get_number(json, "missing_key", v));
}

/**
 * Control plane: parse_config_json applies only newer config versions.
 */
TEST(ConfigParse, AppliesOnlyNewerVersions) {
    config::DeviceConfig cfg = config::default_config();
    cfg.config_version = 3;

    // Older / equal version is ignored.
    EXPECT_FALSE(connectivity::parse_config_json("{\"config_version\":3}", cfg));

    // Newer version updates fields.
    const char* doc = "{\"config_version\":4,\"sleep_duration_us\":600000000,"
                      "\"heartbeat_interval\":288,\"sample_rate_hz\":100,\"anomaly_k\":2.0}";
    EXPECT_TRUE(connectivity::parse_config_json(doc, cfg));
    EXPECT_EQ(cfg.config_version, 4u);
    EXPECT_EQ(cfg.sleep_duration_us, 600000000u);
    EXPECT_EQ(cfg.heartbeat_interval, 288u);
    EXPECT_EQ(cfg.sample_rate_hz, 100u);
    EXPECT_NEAR(cfg.anomaly_k, 2.0f, 1e-6f);
}

/**
 * Control plane: fetch_config (host mock) applies a queued config document.
 */
TEST(ConfigParse, FetchConfigViaMock) {
    connectivity::mock::reset();
    config::DeviceConfig cfg = config::default_config();  // version 0
    connectivity::mock::set_config_json("{\"config_version\":1,\"heartbeat_interval\":99}");

    EXPECT_TRUE(connectivity::fetch_config("http://example.com/config", cfg));
    EXPECT_EQ(cfg.config_version, 1u);
    EXPECT_EQ(cfg.heartbeat_interval, 99u);
}
