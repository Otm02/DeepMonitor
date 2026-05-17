#include <gtest/gtest.h>
#include <cstring>

#include "rtc.h"
#include "nvs.h"
#include "ml.h"

/**
 * Unit tests for rtc::restore_state() — Task 6.3
 * Tests the three wake paths: WARM_WAKE, HARD_BOOT_NVS, HARD_BOOT_FRESH
 *
 * Validates: Requirements 6.2, 6.4
 */

class RestoreStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        persist::reset_mock();
        memset(&block, 0, sizeof(block));
    }

    rtc::RtcBlock block;
};

// WARM_WAKE: valid CRC → state used as-is
TEST_F(RestoreStateTest, ValidCrcReturnsWarmWake) {
    block.duty_cycle_count = 42;
    block.ml_weights.learning_rate = 0.005f;
    rtc::save(block);  // compute valid CRC

    auto wake = rtc::restore_state(block);

    EXPECT_EQ(wake, rtc::WakeType::WARM_WAKE);
    EXPECT_EQ(block.duty_cycle_count, 42u);
    EXPECT_FLOAT_EQ(block.ml_weights.learning_rate, 0.005f);
}

// HARD_BOOT_NVS: invalid CRC + valid NVS data → restore from NVS
TEST_F(RestoreStateTest, InvalidCrcWithNvsReturnsHardBootNvs) {
    // Prepare NVS with known data
    ml::Weights saved_weights;
    memset(&saved_weights, 0, sizeof(saved_weights));
    saved_weights.learning_rate = 0.007f;
    saved_weights.encoder[0][0] = 1.23f;

    normalizer::Stats saved_stats;
    saved_stats.mean_vib = 2.0f;
    saved_stats.var_vib = 0.5f;
    saved_stats.mean_temp = 1.0f;
    saved_stats.var_temp = 0.3f;

    persist::save(saved_weights, saved_stats);

    // Block has invalid CRC (all zeros, CRC not computed)
    block.duty_cycle_count = 99;
    block.crc16 = 0xDEAD;  // wrong CRC

    auto wake = rtc::restore_state(block);

    EXPECT_EQ(wake, rtc::WakeType::HARD_BOOT_NVS);
    // ML weights and stats restored from NVS
    EXPECT_FLOAT_EQ(block.ml_weights.learning_rate, 0.007f);
    EXPECT_FLOAT_EQ(block.ml_weights.encoder[0][0], 1.23f);
    EXPECT_FLOAT_EQ(block.norm_stats.mean_vib, 2.0f);
    EXPECT_FLOAT_EQ(block.norm_stats.var_vib, 0.5f);
    // Other fields reset to defaults
    EXPECT_EQ(block.duty_cycle_count, 0u);
    EXPECT_EQ(block.last_nvs_save_cycle, 0u);
    EXPECT_EQ(block.last_heartbeat_cycle, 0u);
    EXPECT_EQ(block.wifi_fail_count, 0);
    EXPECT_EQ(block.http_retry_count, 0);
    EXPECT_EQ(block.imu_fail_logged, 0);
    EXPECT_EQ(block.window_state.head, 0);
    EXPECT_EQ(block.window_state.count, 0);
}

// HARD_BOOT_FRESH: invalid CRC + no NVS data → fresh init
TEST_F(RestoreStateTest, InvalidCrcNoNvsReturnsHardBootFresh) {
    // No NVS data saved, block has bad CRC
    block.crc16 = 0xBEEF;

    auto wake = rtc::restore_state(block);

    EXPECT_EQ(wake, rtc::WakeType::HARD_BOOT_FRESH);
    // All counters reset
    EXPECT_EQ(block.duty_cycle_count, 0u);
    EXPECT_EQ(block.last_nvs_save_cycle, 0u);
    EXPECT_EQ(block.last_heartbeat_cycle, 0u);
    EXPECT_EQ(block.wifi_fail_count, 0);
    EXPECT_EQ(block.http_retry_count, 0);
    EXPECT_EQ(block.imu_fail_logged, 0);
    // Norm stats zeroed
    EXPECT_FLOAT_EQ(block.norm_stats.mean_vib, 0.0f);
    EXPECT_FLOAT_EQ(block.norm_stats.var_vib, 0.0f);
    // Window state zeroed
    EXPECT_EQ(block.window_state.head, 0);
    EXPECT_EQ(block.window_state.count, 0);
    // ML weights initialized (learning_rate should be LR_INITIAL)
    EXPECT_FLOAT_EQ(block.ml_weights.learning_rate, config::LR_INITIAL);
    EXPECT_EQ(block.ml_weights.normal_cycle_count, 0);
    EXPECT_EQ(block.ml_weights.anomaly_streak, 0);
}

/**
 * Unit tests for rtc::enter_deep_sleep() — Task 6.4
 * Under HOST_TEST, enter_deep_sleep calls save() and returns.
 *
 * Validates: Requirements 5.1, 5.3
 */

class DeepSleepTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&block, 0, sizeof(block));
    }

    rtc::RtcBlock block;
};

// enter_deep_sleep persists state with valid CRC
TEST_F(DeepSleepTest, PersistsStateWithValidCrc) {
    block.duty_cycle_count = 100;
    block.ml_weights.learning_rate = 0.008f;
    block.norm_stats.mean_vib = 1.5f;

    rtc::enter_deep_sleep(block);

    // After enter_deep_sleep, the block should have a valid CRC
    EXPECT_TRUE(rtc::validate(block));
}

// enter_deep_sleep preserves all field values
TEST_F(DeepSleepTest, PreservesFieldValues) {
    block.duty_cycle_count = 55;
    block.last_nvs_save_cycle = 10;
    block.last_heartbeat_cycle = 20;
    block.wifi_fail_count = 3;
    block.http_retry_count = 1;
    block.ml_weights.encoder[0][0] = 0.42f;

    rtc::enter_deep_sleep(block);

    EXPECT_EQ(block.duty_cycle_count, 55u);
    EXPECT_EQ(block.last_nvs_save_cycle, 10u);
    EXPECT_EQ(block.last_heartbeat_cycle, 20u);
    EXPECT_EQ(block.wifi_fail_count, 3);
    EXPECT_EQ(block.http_retry_count, 1);
    EXPECT_FLOAT_EQ(block.ml_weights.encoder[0][0], 0.42f);
    EXPECT_TRUE(rtc::validate(block));
}
