#include <gtest/gtest.h>
#include <cmath>
#include "imu.h"

// Access mock controls declared in imu.cpp
namespace imu { namespace mock {
    void set_should_fail(bool fail);
    void reset();
}}

class ImuTest : public ::testing::Test {
protected:
    void SetUp() override {
        imu::mock::reset();
    }
    void TearDown() override {
        imu::mock::reset();
    }
};

TEST_F(ImuTest, InitSucceeds) {
    EXPECT_TRUE(imu::init());
}

TEST_F(ImuTest, InitFailsWhenMockSetToFail) {
    imu::mock::set_should_fail(true);
    EXPECT_FALSE(imu::init());
}

TEST_F(ImuTest, SampleProducesExactly500Entries) {
    imu::init();
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    imu::sample(vib, temp);

    // Verify all 500 entries are populated with finite values
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        EXPECT_TRUE(std::isfinite(vib[i])) << "vib[" << i << "] is not finite";
        EXPECT_TRUE(std::isfinite(temp[i])) << "temp[" << i << "] is not finite";
    }
}

TEST_F(ImuTest, SampleVibrationMagnitudeIsCorrect) {
    imu::init();
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    imu::sample(vib, temp);

    // Verify vibration magnitude matches sqrt(ax^2 + ay^2 + az^2)
    // Mock data: ax = 0.001*i, ay = 0.002*i, az = 0.003*i
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        float ax = 0.001f * static_cast<float>(i);
        float ay = 0.002f * static_cast<float>(i);
        float az = 0.003f * static_cast<float>(i);
        float expected = std::sqrt(ax * ax + ay * ay + az * az);
        EXPECT_NEAR(vib[i], expected, 1e-6f) << "Mismatch at sample " << i;
    }
}

TEST_F(ImuTest, SampleTemperatureIsReasonable) {
    imu::init();
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    imu::sample(vib, temp);

    // Mock data: temp = 25.0 + 0.01*i
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        float expected = 25.0f + 0.01f * static_cast<float>(i);
        EXPECT_NEAR(temp[i], expected, 1e-6f) << "Mismatch at sample " << i;
    }
}

TEST_F(ImuTest, FirstSampleVibrationIsZero) {
    imu::init();
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    imu::sample(vib, temp);

    // At i=0, all accelerometer components are 0, so magnitude should be 0
    EXPECT_FLOAT_EQ(vib[0], 0.0f);
}

TEST_F(ImuTest, VibrationMagnitudeIsNonNegative) {
    imu::init();
    float vib[config::SAMPLE_COUNT];
    float temp[config::SAMPLE_COUNT];
    imu::sample(vib, temp);

    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        EXPECT_GE(vib[i], 0.0f) << "Negative vibration at sample " << i;
    }
}
