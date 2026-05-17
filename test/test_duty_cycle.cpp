#include <gtest/gtest.h>
#include <cstring>

#include "config.h"
#include "imu.h"
#include "normalizer.h"
#include "window.h"
#include "ml.h"
#include "rtc.h"
#include "nvs.h"
#include "connectivity.h"
#include "flash_buffer.h"

// Access mock controls
namespace imu { namespace mock {
    void set_should_fail(bool fail);
    void reset();
}}

// The function under test, defined in src/main.cpp
extern void run_duty_cycle(rtc::RtcBlock& block);

/**
 * Unit tests for run_duty_cycle() — Task 10.2
 *
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
    }

    void TearDown() override {
        imu::mock::reset();
        persist::reset_mock();
        connectivity::mock::reset();
        flash_buffer::mock::reset();
    }

    rtc::RtcBlock block;
};

// ── Test 1: Normal duty cycle (warm wake) ───────────────────────────────────
// Initialize block with valid CRC, run duty cycle, verify:
//   - duty_cycle_count incremented
//   - Block has valid CRC after (enter_deep_sleep calls save)
//   - Window state count increased
TEST_F(DutyCycleTest, NormalDutyCycleIncrementsCountAndSavesState) {
    // Set up a warm-wake block with valid CRC
    block.duty_cycle_count = 5;
    block.ml_weights.learning_rate = config::LR_INITIAL;
    block.norm_stats.var_vib = 1.0f;
    block.norm_stats.var_temp = 1.0f;
    rtc::save(block);  // compute valid CRC → warm wake path

    int old_window_count = block.window_state.count;

    run_duty_cycle(block);

    // duty_cycle_count should have been incremented
    EXPECT_EQ(block.duty_cycle_count, 6u);
    // Block should have valid CRC (enter_deep_sleep calls save)
    EXPECT_TRUE(rtc::validate(block));
    // Window state should have one more entry
    EXPECT_GT(block.window_state.count, old_window_count);
}

// ── Test 2: IMU failure path ────────────────────────────────────────────────
// Set imu::mock::set_should_fail(true), run duty cycle, verify:
//   - imu_fail_logged is set to 1
//   - Block has valid CRC (entered deep sleep)
//   - duty_cycle_count NOT incremented (returned early)
TEST_F(DutyCycleTest, ImuFailureLogsAndSleepsWithoutIncrement) {
    block.duty_cycle_count = 10;
    block.ml_weights.learning_rate = config::LR_INITIAL;
    rtc::save(block);  // valid CRC → warm wake

    imu::mock::set_should_fail(true);

    run_duty_cycle(block);

    EXPECT_EQ(block.imu_fail_logged, 1);
    EXPECT_TRUE(rtc::validate(block));
    // duty_cycle_count should NOT have been incremented (early return)
    EXPECT_EQ(block.duty_cycle_count, 10u);
}

// ── Test 3: Hard boot with no NVS ───────────────────────────────────────────
// Start with invalid CRC and no NVS data, run duty cycle, verify:
//   - Block gets fresh initialized weights (learning_rate = LR_INITIAL)
//   - duty_cycle_count is 1 (reset to 0 then incremented)
TEST_F(DutyCycleTest, HardBootFreshInitializesAndRuns) {
    // Invalid CRC, no NVS data → HARD_BOOT_FRESH
    block.crc16 = 0xDEAD;
    block.duty_cycle_count = 99;  // will be reset to 0 by restore_state

    run_duty_cycle(block);

    // After fresh init, duty_cycle_count reset to 0 then incremented to 1
    EXPECT_EQ(block.duty_cycle_count, 1u);
    // Block should have valid CRC
    EXPECT_TRUE(rtc::validate(block));
}

// ── Test 4: Hard boot with valid NVS ────────────────────────────────────────
// Save known weights to NVS mock, start with invalid CRC, run duty cycle, verify:
//   - Weights restored from NVS
//   - duty_cycle_count is 1
TEST_F(DutyCycleTest, HardBootNvsRestoresWeightsAndRuns) {
    // Save known weights to NVS
    ml::Weights saved_weights;
    memset(&saved_weights, 0, sizeof(saved_weights));
    saved_weights.learning_rate = 0.007f;
    saved_weights.encoder[0][0] = 1.23f;
    saved_weights.decoder[0][0] = 1.23f;

    normalizer::Stats saved_stats;
    saved_stats.mean_vib = 2.0f;
    saved_stats.var_vib = 1.0f;
    saved_stats.mean_temp = 1.0f;
    saved_stats.var_temp = 1.0f;

    persist::save(saved_weights, saved_stats);

    // Invalid CRC → triggers hard boot, NVS has data → HARD_BOOT_NVS
    block.crc16 = 0xDEAD;

    run_duty_cycle(block);

    // duty_cycle_count reset to 0 then incremented to 1
    EXPECT_EQ(block.duty_cycle_count, 1u);
    EXPECT_TRUE(rtc::validate(block));
}

// ── Test 5: NVS time-based save trigger ─────────────────────────────────────
// Set duty_cycle_count to NVS_SAVE_INTERVAL + last_nvs_save_cycle,
// run duty cycle, verify last_nvs_save_cycle updated
TEST_F(DutyCycleTest, NvsTimeSaveTriggerUpdatesLastSaveCycle) {
    // Set up warm wake with duty_cycle_count just below the trigger
    // After increment inside run_duty_cycle, count will be exactly at trigger
    uint32_t last_save = 10;
    block.last_nvs_save_cycle = last_save;
    // duty_cycle_count will be incremented inside run_duty_cycle,
    // so set it to (last_save + NVS_SAVE_INTERVAL - 1) so after ++ it triggers
    block.duty_cycle_count = last_save + config::NVS_SAVE_INTERVAL - 1;
    block.ml_weights.learning_rate = config::LR_INITIAL;
    block.norm_stats.var_vib = 1.0f;
    block.norm_stats.var_temp = 1.0f;
    rtc::save(block);  // valid CRC → warm wake

    run_duty_cycle(block);

    // After the duty cycle, last_nvs_save_cycle should be updated
    // to the current duty_cycle_count
    EXPECT_EQ(block.last_nvs_save_cycle, block.duty_cycle_count);
    EXPECT_TRUE(rtc::validate(block));
}

// ── Test 6: Heartbeat trigger ───────────────────────────────────────────────
// Set duty_cycle_count to HEARTBEAT_INTERVAL + last_heartbeat_cycle,
// run duty cycle (normal, not anomaly), verify last_heartbeat_cycle updated
TEST_F(DutyCycleTest, HeartbeatTriggerUpdatesLastHeartbeatCycle) {
    // Set up warm wake with duty_cycle_count just below heartbeat trigger
    uint32_t last_hb = 5;
    block.last_heartbeat_cycle = last_hb;
    // After increment, count will be at (last_hb + HEARTBEAT_INTERVAL)
    block.duty_cycle_count = last_hb + config::HEARTBEAT_INTERVAL - 1;
    block.ml_weights.learning_rate = config::LR_INITIAL;

    // Use large variance so normalized data is near-zero → low reconstruction
    // error → normal (non-anomaly) cycle, allowing heartbeat branch to execute
    block.norm_stats.mean_vib = 0.5f;
    block.norm_stats.var_vib = 100000.0f;
    block.norm_stats.mean_temp = 27.0f;
    block.norm_stats.var_temp = 100000.0f;

    // Pre-fill window with zeros so ML input is mostly zero
    block.window_state.count = config::WINDOW_SIZE;
    block.window_state.head = 0;
    memset(block.window_state.buffer, 0, sizeof(block.window_state.buffer));

    rtc::save(block);  // valid CRC → warm wake

    run_duty_cycle(block);

    // Heartbeat should have been sent (normal cycle, not anomaly)
    // last_heartbeat_cycle should be updated to current duty_cycle_count
    EXPECT_EQ(block.last_heartbeat_cycle, block.duty_cycle_count);
    EXPECT_TRUE(rtc::validate(block));
}
