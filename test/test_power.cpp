#include <gtest/gtest.h>
#include "power.h"

TEST(PowerTest, HostMockReturnsReasonableVoltage) {
    float v = power::read_battery_voltage();
    EXPECT_GT(v, 3.0f);
    EXPECT_LT(v, 5.0f);
}
