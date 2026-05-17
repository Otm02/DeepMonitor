#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cstring>
#include "connectivity.h"
#include "config.h"

/**
 * Property 13: Alert Payload Contains All Required Fields
 *
 * For any AlertPayload with valid field values, the serialized JSON string
 * should contain the keys: type, device_id, timestamp_ms, reconstruction_error,
 * anomaly_threshold, mean_vibration, mean_temperature_delta, and battery_voltage.
 *
 * **Validates: Requirements 8.2**
 */
RC_GTEST_PROP(AlertPayload, ContainsAllRequiredFields, ()) {
    connectivity::mock::reset();

    const auto timestamp = static_cast<uint32_t>(*rc::gen::inRange(0, 1000000));
    const float recon_err = static_cast<float>(*rc::gen::inRange(0, 10000)) / 1000.0f;
    const float threshold = static_cast<float>(*rc::gen::inRange(1, 1000)) / 1000.0f;
    const float mean_vib = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    const float mean_temp = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    const float battery = static_cast<float>(*rc::gen::inRange(300, 420)) / 100.0f;

    connectivity::AlertPayload payload;
    payload.device_id = config::DEVICE_ID_STR;
    payload.timestamp_ms = timestamp;
    payload.reconstruction_error = recon_err;
    payload.anomaly_threshold = threshold;
    payload.mean_vib = mean_vib;
    payload.mean_temp = mean_temp;
    payload.battery_voltage = battery;

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
 * Property 14: HTTP Retry Counter Increments on Failure
 *
 * For any initial retry count, when an HTTP POST fails, the send_alert
 * function should return false. This validates the failure path behavior
 * of the connectivity module.
 *
 * **Validates: Requirements 8.4**
 */
RC_GTEST_PROP(HTTPRetry, SendAlertReturnsFalseOnHTTPFailure, ()) {
    connectivity::mock::reset();
    connectivity::mock::set_http_should_fail(true);

    const auto timestamp = static_cast<uint32_t>(*rc::gen::inRange(0, 1000000));
    const float recon_err = static_cast<float>(*rc::gen::inRange(0, 10000)) / 1000.0f;
    const float threshold = static_cast<float>(*rc::gen::inRange(1, 1000)) / 1000.0f;
    const float mean_vib = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    const float mean_temp = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    const float battery = static_cast<float>(*rc::gen::inRange(300, 420)) / 100.0f;

    connectivity::AlertPayload payload;
    payload.device_id = config::DEVICE_ID_STR;
    payload.timestamp_ms = timestamp;
    payload.reconstruction_error = recon_err;
    payload.anomaly_threshold = threshold;
    payload.mean_vib = mean_vib;
    payload.mean_temp = mean_temp;
    payload.battery_voltage = battery;

    bool result = connectivity::send_alert(payload, "http://example.com/webhook");
    RC_ASSERT(!result);

    // JSON should still be serialized even on failure
    const char* json = connectivity::mock::get_last_json();
    RC_ASSERT(std::strlen(json) > 0);
}

/**
 * Property 15: Heartbeat Interval Trigger
 *
 * For any duty cycle count and last heartbeat cycle, the heartbeat should
 * be triggered if and only if
 * (duty_cycle_count - last_heartbeat_cycle) >= HEARTBEAT_INTERVAL.
 *
 * **Validates: Requirements 9.1**
 */
RC_GTEST_PROP(HeartbeatInterval, TriggerMatchesIntervalCondition, ()) {
    const auto last_heartbeat_cycle = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    const auto offset = static_cast<uint32_t>(*rc::gen::inRange(0, 100000));
    const uint32_t duty_cycle_count = last_heartbeat_cycle + offset;

    const bool result = connectivity::should_send_heartbeat(duty_cycle_count, last_heartbeat_cycle);
    const bool expected = (duty_cycle_count - last_heartbeat_cycle) >=
                          static_cast<uint32_t>(config::HEARTBEAT_INTERVAL);

    RC_ASSERT(result == expected);
}

/**
 * Property 16: Heartbeat Payload Contains All Required Fields
 *
 * For any HeartbeatPayload with valid field values, the serialized JSON string
 * should contain the keys: type, device_id, uptime_cycles, battery_voltage,
 * anomaly_threshold, norm_mean_vib, norm_var_vib, norm_mean_temp,
 * norm_var_temp, and weight_magnitude.
 *
 * **Validates: Requirements 9.2**
 */
RC_GTEST_PROP(HeartbeatPayload, ContainsAllRequiredFields, ()) {
    connectivity::mock::reset();

    const auto uptime = static_cast<uint32_t>(*rc::gen::inRange(0, 1000000));
    const float battery = static_cast<float>(*rc::gen::inRange(300, 420)) / 100.0f;
    const float threshold = static_cast<float>(*rc::gen::inRange(1, 1000)) / 1000.0f;
    const float mean_vib = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    const float var_vib = static_cast<float>(*rc::gen::inRange(1, 10000)) / 1000.0f;
    const float mean_temp = static_cast<float>(*rc::gen::inRange(-10000, 10001)) / 1000.0f;
    const float var_temp = static_cast<float>(*rc::gen::inRange(1, 10000)) / 1000.0f;
    const float weight_mag = static_cast<float>(*rc::gen::inRange(0, 100000)) / 100.0f;

    connectivity::HeartbeatPayload payload;
    payload.device_id = config::DEVICE_ID_STR;
    payload.uptime_cycles = uptime;
    payload.battery_voltage = battery;
    payload.anomaly_threshold = threshold;
    payload.norm_stats.mean_vib = mean_vib;
    payload.norm_stats.var_vib = var_vib;
    payload.norm_stats.mean_temp = mean_temp;
    payload.norm_stats.var_temp = var_temp;
    payload.weight_magnitude_summary = weight_mag;

    connectivity::send_heartbeat(payload, "http://example.com/webhook");

    const char* json = connectivity::mock::get_last_json();
    RC_ASSERT(std::strlen(json) > 0);

    RC_ASSERT(std::strstr(json, "\"type\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"device_id\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"uptime_cycles\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"battery_voltage\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"anomaly_threshold\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"norm_mean_vib\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"norm_var_vib\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"norm_mean_temp\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"norm_var_temp\"") != nullptr);
    RC_ASSERT(std::strstr(json, "\"weight_magnitude\"") != nullptr);
}
