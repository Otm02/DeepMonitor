#ifndef RTC_H
#define RTC_H

#include <cstdint>
#include <cstddef>
#include "config.h"
#include "normalizer.h"
#include "ml.h"

namespace rtc {
    struct RtcBlock {
        normalizer::Stats norm_stats;       // per-feature baseline
        ml::Weights ml_weights;             // autoencoder parameters
        ml::ReconTracker recon;             // adaptive-threshold history
        config::DeviceConfig cfg;           // runtime config (control plane)
        float last_features[config::FEATURE_DIM]; // most recent feature vector (for telemetry)
        float last_recon_error;             // most recent reconstruction error
        uint32_t duty_cycle_count;
        uint32_t last_nvs_save_cycle;
        uint32_t last_heartbeat_cycle;
        uint8_t wifi_fail_count;
        uint8_t http_retry_count;
        uint8_t imu_fail_logged;
        uint16_t crc16;
    };

    enum class WakeType { WARM_WAKE, HARD_BOOT_NVS, HARD_BOOT_FRESH };

    bool validate(const RtcBlock& block);
    void save(RtcBlock& block);
    uint16_t compute_crc16(const uint8_t* data, size_t len);
    WakeType restore_state(RtcBlock& block);
    void enter_deep_sleep(RtcBlock& block);
}

#endif // RTC_H
