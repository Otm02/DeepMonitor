#include <gtest/gtest.h>
#include <cstring>

#include "config.h"
#include "imu.h"
#include "normalizer.h"
#include "ml.h"
#include "rtc.h"
#include "nvs.h"
#include "connectivity.h"
#include "flash_buffer.h"

namespace imu { namespace mock {
    void set_should_fail(bool fail);
    void reset();
}}

extern void run_duty_cycle(rtc::RtcBlock& block);

/**
 * Unit tests for run_duty_cycle() orchestration.
 * Validates: Requirements 5.2, 7.1, 7.2, 9.1
 */
class DutyCycleTest : public ::testing::Test {
protected:
    void SetUp() override {
        imu::mock::reset();
        persist::reset_mock();
        connectivity::mock::reset();
        flash_buffer::mock::reset();
        memset(&block, 0, sizeof(block));
        block.cfg = config::default_config();
    }

    void TearDown() override {
        imu::mock::reset();
        persist::reset_mock();
        connectivity::mock::reset();
        flash_buffer::mock::reset();
    }

    rtc::RtcBlock block;
};

// Normal warm-wake cycle: count increments, baseline learns, state re-saved.
TEST_F(DutyCycleTest, NormalDutyCycleIncrementsCountAndLearns) {
    block.duty_cycle_count = 5;
    block.ml_weights.learning_rate = config::LR_INITIAL;
    rtc::save(block);  // valid CRC → warm wake

    run_duty_cycle(block);

    EXPECT_EQ(block.duty_cycle_count, 6u);
    EXPECT_TRUE(rtc::validate(block));
    // Normalizer learned from this cycle's feature vector.
    EXPECT_GT(block.norm_stats.count, 0u);
}

// IMU failure path: logs the flag, sleeps, does not advance the cycle.
TEST_F(DutyCycleTest, ImuFailureLogsAndSleepsWithoutIncrement) {
    block.duty_cycle_count = 10;
    block.ml_weights.learning_rate = config::LR_INITIAL;
    rtc::save(block);

    imu::mock::set_should_fail(true);
    run_duty_cycle(block);

    EXPECT_EQ(block.imu_fail_logged, 1);
    EXPECT_TRUE(rtc::validate(block));
    EXPECT_EQ(block.duty_cycle_count, 10u);
}

// Hard boot with no NVS → fresh init, count becomes 1.
TEST_F(DutyCycleTest, HardBootFreshInitializesAndRuns) {
    block.crc16 = 0xDEAD;
    block.duty_cycle_count = 99;

    run_duty_cycle(block);

    EXPECT_EQ(block.duty_cycle_count, 1u);
    EXPECT_TRUE(rtc::validate(block));
}

// Hard boot with valid NVS → weights restored, count becomes 1.
TEST_F(DutyCycleTest, HardBootNvsRestoresWeightsAndRuns) {
    ml::Weights saved_weights;
    memset(&saved_weights, 0, sizeof(saved_weights));
    saved_weights.learning_rate = 0.007f;
    saved_weights.W1[0][0] = 1.23f;

    normalizer::Stats saved_stats;
    memset(&saved_stats, 0, sizeof(saved_stats));
    saved_stats.count = 50;
    saved_stats.mean[0] = 2.0f;
    saved_stats.var[0] = 1.0f;

    persist::save(saved_weights, saved_stats);

    block.crc16 = 0xDEAD;
    run_duty_cycle(block);

    EXPECT_EQ(block.duty_cycle_count, 1u);
    EXPECT_TRUE(rtc::validate(block));
}

// NVS time-based save trigger updates last_nvs_save_cycle.
TEST_F(DutyCycleTest, NvsTimeSaveTriggerUpdatesLastSaveCycle) {
    uint32_t last_save = 10;
    block.last_nvs_save_cycle = last_save;
    block.duty_cycle_count = last_save + config::NVS_SAVE_INTERVAL - 1;
    block.ml_weights.learning_rate = config::LR_INITIAL;
    rtc::save(block);

    run_duty_cycle(block);

    EXPECT_EQ(block.last_nvs_save_cycle, block.duty_cycle_count);
    EXPECT_TRUE(rtc::validate(block));
}

// Heartbeat trigger updates last_heartbeat_cycle on a normal cycle.
TEST_F(DutyCycleTest, HeartbeatTriggerUpdatesLastHeartbeatCycle) {
    uint32_t last_hb = 5;
    block.last_heartbeat_cycle = last_hb;
    block.duty_cycle_count = last_hb + block.cfg.heartbeat_interval - 1;
    block.ml_weights.learning_rate = config::LR_INITIAL;
    // During warmup the adaptive threshold suppresses anomalies, so this is a
    // guaranteed normal cycle and the heartbeat branch runs.
    rtc::save(block);

    run_duty_cycle(block);

    EXPECT_EQ(block.last_heartbeat_cycle, block.duty_cycle_count);
    EXPECT_TRUE(rtc::validate(block));
}
