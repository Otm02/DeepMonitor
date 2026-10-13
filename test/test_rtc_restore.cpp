#include <gtest/gtest.h>
#include <cstring>

#include "rtc.h"
#include "nvs.h"
#include "ml.h"

/**
 * Unit tests for rtc::restore_state() — WARM_WAKE / HARD_BOOT_NVS / HARD_BOOT_FRESH.
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

TEST_F(RestoreStateTest, ValidCrcReturnsWarmWake) {
    block.duty_cycle_count = 42;
    block.ml_weights.learning_rate = 0.005f;
    rtc::save(block);

    auto wake = rtc::restore_state(block);

    EXPECT_EQ(wake, rtc::WakeType::WARM_WAKE);
    EXPECT_EQ(block.duty_cycle_count, 42u);
    EXPECT_FLOAT_EQ(block.ml_weights.learning_rate, 0.005f);
}

TEST_F(RestoreStateTest, InvalidCrcWithNvsReturnsHardBootNvs) {
    ml::Weights saved_weights;
    memset(&saved_weights, 0, sizeof(saved_weights));
    saved_weights.learning_rate = 0.007f;
    saved_weights.W1[0][0] = 1.23f;

    normalizer::Stats saved_stats;
    memset(&saved_stats, 0, sizeof(saved_stats));
    saved_stats.count = 12;
    saved_stats.mean[0] = 2.0f;
    saved_stats.var[0] = 0.5f;

    persist::save(saved_weights, saved_stats);

    block.duty_cycle_count = 99;
    block.crc16 = 0xDEAD;

    auto wake = rtc::restore_state(block);

    EXPECT_EQ(wake, rtc::WakeType::HARD_BOOT_NVS);
    EXPECT_FLOAT_EQ(block.ml_weights.learning_rate, 0.007f);
    EXPECT_FLOAT_EQ(block.ml_weights.W1[0][0], 1.23f);
    EXPECT_FLOAT_EQ(block.norm_stats.mean[0], 2.0f);
    EXPECT_FLOAT_EQ(block.norm_stats.var[0], 0.5f);
    // Runtime state reset to defaults.
    EXPECT_EQ(block.duty_cycle_count, 0u);
    EXPECT_EQ(block.last_nvs_save_cycle, 0u);
    EXPECT_EQ(block.last_heartbeat_cycle, 0u);
    EXPECT_EQ(block.wifi_fail_count, 0);
    EXPECT_EQ(block.http_retry_count, 0);
    EXPECT_EQ(block.imu_fail_logged, 0);
    EXPECT_EQ(block.recon.count, 0);
    EXPECT_EQ(block.cfg.heartbeat_interval, config::HEARTBEAT_INTERVAL);
}

TEST_F(RestoreStateTest, InvalidCrcNoNvsReturnsHardBootFresh) {
    block.crc16 = 0xBEEF;

    auto wake = rtc::restore_state(block);

    EXPECT_EQ(wake, rtc::WakeType::HARD_BOOT_FRESH);
    EXPECT_EQ(block.duty_cycle_count, 0u);
    EXPECT_EQ(block.last_nvs_save_cycle, 0u);
    EXPECT_EQ(block.last_heartbeat_cycle, 0u);
    EXPECT_EQ(block.wifi_fail_count, 0);
    EXPECT_EQ(block.http_retry_count, 0);
    EXPECT_EQ(block.imu_fail_logged, 0);
    EXPECT_FLOAT_EQ(block.norm_stats.mean[0], 0.0f);
    EXPECT_EQ(block.norm_stats.count, 0u);
    EXPECT_EQ(block.recon.count, 0);
    EXPECT_FLOAT_EQ(block.ml_weights.learning_rate, config::LR_INITIAL);
    EXPECT_EQ(block.ml_weights.normal_cycle_count, 0);
    EXPECT_EQ(block.ml_weights.anomaly_streak, 0);
    EXPECT_EQ(block.cfg.sleep_duration_us, config::SLEEP_DURATION_US);
}

/**
 * Unit tests for rtc::enter_deep_sleep() — under HOST_TEST it saves and returns.
 * Validates: Requirements 5.1, 5.3
 */
class DeepSleepTest : public ::testing::Test {
protected:
    void SetUp() override {
        memset(&block, 0, sizeof(block));
        block.cfg = config::default_config();
    }
    rtc::RtcBlock block;
};

TEST_F(DeepSleepTest, PersistsStateWithValidCrc) {
    block.duty_cycle_count = 100;
    block.ml_weights.learning_rate = 0.008f;
    block.norm_stats.mean[0] = 1.5f;

    rtc::enter_deep_sleep(block);
    EXPECT_TRUE(rtc::validate(block));
}

TEST_F(DeepSleepTest, PreservesFieldValues) {
    block.duty_cycle_count = 55;
    block.last_nvs_save_cycle = 10;
    block.last_heartbeat_cycle = 20;
    block.wifi_fail_count = 3;
    block.http_retry_count = 1;
    block.ml_weights.W1[0][0] = 0.42f;

    rtc::enter_deep_sleep(block);

    EXPECT_EQ(block.duty_cycle_count, 55u);
    EXPECT_EQ(block.last_nvs_save_cycle, 10u);
    EXPECT_EQ(block.last_heartbeat_cycle, 20u);
    EXPECT_EQ(block.wifi_fail_count, 3);
    EXPECT_EQ(block.http_retry_count, 1);
    EXPECT_FLOAT_EQ(block.ml_weights.W1[0][0], 0.42f);
    EXPECT_TRUE(rtc::validate(block));
}
