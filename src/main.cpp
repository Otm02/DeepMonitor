#include "config.h"
#include "board.h"
#include "imu.h"
#include "normalizer.h"
#include "window.h"
#include "ml.h"
#include "rtc.h"
#include "nvs.h"
#include "connectivity.h"
#include "power.h"
#include "flash_buffer.h"

extern rtc::RtcBlock rtc_block;

static float s_vib[config::SAMPLE_COUNT];
static float s_temp[config::SAMPLE_COUNT];
static float s_norm_vib[config::SAMPLE_COUNT];
static float s_norm_temp[config::SAMPLE_COUNT];

static ml::Weights s_saved_weights;

#ifndef HOST_TEST
#include <Arduino.h>
#if HAS_M5_POWER
#include <M5Unified.h>
#endif
#endif

namespace {
bool post_buffered_json(const char* json) {
    return connectivity::post_json(json, config::WEBHOOK_URL_STR);
}

void try_drain_flash_buffer() {
    if (flash_buffer::pending_count() == 0) {
        return;
    }
    if (!connectivity::connect_wifi(config::WIFI_TIMEOUT_MS)) {
        return;
    }
    flash_buffer::drain(post_buffered_json);
    connectivity::disconnect_wifi();
}

void buffer_last_payload_on_failure(bool sent_ok) {
    if (!sent_ok) {
        flash_buffer::enqueue(connectivity::get_last_json());
    }
}
} // namespace

void run_duty_cycle(rtc::RtcBlock& block) {
    rtc::WakeType wake = rtc::restore_state(block);

    if (wake != rtc::WakeType::WARM_WAKE) {
        s_saved_weights = block.ml_weights;
    }

#ifndef HOST_TEST
#if HAS_M5_POWER
    auto cfg = M5.config();
    M5.begin(cfg);
#endif
    flash_buffer::init();
#endif

    if (!imu::init()) {
        block.imu_fail_logged = 1;
        rtc::enter_deep_sleep(block);
        return;
    }

    imu::sample(s_vib, s_temp);

    normalizer::update(s_vib, s_temp, block.norm_stats, config::SMOOTHING_ALPHA);
    normalizer::normalize(s_vib, s_temp, block.norm_stats, s_norm_vib, s_norm_temp);

    float batch_mean_vib = 0.0f;
    float batch_mean_temp = 0.0f;
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        batch_mean_vib += s_norm_vib[i];
        batch_mean_temp += s_norm_temp[i];
    }
    batch_mean_vib /= config::SAMPLE_COUNT;
    batch_mean_temp /= config::SAMPLE_COUNT;
    window::append(block.window_state, batch_mean_vib, batch_mean_temp);

    float ml_input[config::INPUT_DIM];
    window::to_flat_array(block.window_state, ml_input);

    ml::InferenceResult result = ml::infer(ml_input, block.ml_weights, config::ANOMALY_THRESHOLD);

    if (result.is_anomaly) {
        normalizer::freeze();
    }
    ml::update_weights(ml_input, result, block.ml_weights);

    block.duty_cycle_count++;

    const float batt_v = power::read_battery_voltage();

#ifndef HOST_TEST
    const uint32_t ts_ms = millis();
#else
    const uint32_t ts_ms = 0;
#endif

    // Attempt to drain any buffered payloads from prior failed uploads
    try_drain_flash_buffer();

    const bool heartbeat_due = connectivity::should_send_heartbeat(
        block.duty_cycle_count, block.last_heartbeat_cycle);
    const bool alert_due = result.is_anomaly;

    if (heartbeat_due || alert_due) {
        if (connectivity::connect_wifi(config::WIFI_TIMEOUT_MS)) {
            if (heartbeat_due) {
                connectivity::HeartbeatPayload hb;
                hb.device_id = config::DEVICE_ID_STR;
                hb.uptime_cycles = block.duty_cycle_count;
                hb.battery_voltage = batt_v;
                hb.anomaly_threshold = config::ANOMALY_THRESHOLD;
                hb.norm_stats = block.norm_stats;
                hb.weight_magnitude_summary = ml::compute_weight_shift(
                    block.ml_weights, s_saved_weights);
                hb.wifi_fail_count = block.wifi_fail_count;
                hb.http_retry_count = block.http_retry_count;
                hb.cycle_reconstruction_error = result.reconstruction_error;
                hb.cycle_is_anomaly = alert_due ? 1 : 0;

                bool hb_sent = connectivity::send_heartbeat(hb, config::WEBHOOK_URL_STR);
                buffer_last_payload_on_failure(hb_sent);
                if (hb_sent) {
                    block.last_heartbeat_cycle = block.duty_cycle_count;
                }
            }

            if (alert_due) {
                float raw_mean_vib = 0.0f;
                float raw_mean_temp = 0.0f;
                for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
                    raw_mean_vib += s_vib[i];
                    raw_mean_temp += s_temp[i];
                }
                raw_mean_vib /= config::SAMPLE_COUNT;
                raw_mean_temp /= config::SAMPLE_COUNT;

                connectivity::AlertPayload alert;
                alert.device_id = config::DEVICE_ID_STR;
                alert.timestamp_ms = ts_ms;
                alert.reconstruction_error = result.reconstruction_error;
                alert.anomaly_threshold = config::ANOMALY_THRESHOLD;
                alert.mean_vib = raw_mean_vib;
                alert.mean_temp = raw_mean_temp;
                alert.battery_voltage = batt_v;

                bool alert_sent = connectivity::send_alert(alert, config::WEBHOOK_URL_STR);
                buffer_last_payload_on_failure(alert_sent);
            }

            connectivity::disconnect_wifi();
        }
    }

    if (persist::should_save_time_based(block.duty_cycle_count, block.last_nvs_save_cycle) ||
        persist::should_save_weight_shift(block.ml_weights, s_saved_weights)) {
        if (persist::save(block.ml_weights, block.norm_stats)) {
            block.last_nvs_save_cycle = block.duty_cycle_count;
            s_saved_weights = block.ml_weights;
        }
    }

    rtc::enter_deep_sleep(block);
}

#ifndef HOST_TEST
void setup() {
    run_duty_cycle(rtc_block);
}

void loop() {
    // Never reached — device enters deep sleep in setup()
}
#endif
