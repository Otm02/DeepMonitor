#include "rtc.h"
#include "board.h"
#include "config.h"
#include "nvs.h"
#include "ml.h"
#include <cstddef>
#include <cstring>

#ifndef HOST_TEST
#include <WiFi.h>
#include <esp_sleep.h>
#if HAS_LCD
#include <M5Unified.h>
#endif
#endif

static_assert(sizeof(rtc::RtcBlock) <= 8192, "RTC memory budget exceeded");

namespace rtc {

uint16_t compute_crc16(const uint8_t* data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
        }
    }
    return crc;
}

void save(RtcBlock& block) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&block);
    size_t crc_len = offsetof(RtcBlock, crc16);
    block.crc16 = compute_crc16(bytes, crc_len);
}

bool validate(const RtcBlock& block) {
    const auto* bytes = reinterpret_cast<const uint8_t*>(&block);
    size_t crc_len = offsetof(RtcBlock, crc16);
    uint16_t computed = compute_crc16(bytes, crc_len);
    return computed == block.crc16;
}

static void reset_runtime_state(RtcBlock& block) {
    memset(&block.recon, 0, sizeof(block.recon));
    memset(block.last_features, 0, sizeof(block.last_features));
    block.last_recon_error = 0.0f;
    block.cfg = config::default_config();
    block.duty_cycle_count = 0;
    block.last_nvs_save_cycle = 0;
    block.last_heartbeat_cycle = 0;
    block.wifi_fail_count = 0;
    block.http_retry_count = 0;
    block.imu_fail_logged = 0;
}

WakeType restore_state(RtcBlock& block) {
    if (validate(block)) {
        return WakeType::WARM_WAKE;
    }
    if (persist::has_valid_data()) {
        persist::load(block.ml_weights, block.norm_stats);
        reset_runtime_state(block);
        return WakeType::HARD_BOOT_NVS;
    }
    ml::init_weights_random(block.ml_weights);
    memset(&block.norm_stats, 0, sizeof(block.norm_stats));
    reset_runtime_state(block);
    return WakeType::HARD_BOOT_FRESH;
}

void enter_deep_sleep(RtcBlock& block) {
    save(block);

#ifndef HOST_TEST
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    btStop();

#if HAS_LCD
    M5.Lcd.sleep();
#endif

    esp_deep_sleep(block.cfg.sleep_duration_us);
#endif
}

} // namespace rtc

#ifndef HOST_TEST
RTC_DATA_ATTR rtc::RtcBlock rtc_block;
#else
rtc::RtcBlock rtc_block;
#endif
