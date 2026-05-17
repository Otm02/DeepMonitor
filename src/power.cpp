#include "power.h"
#include "board.h"

#ifdef HOST_TEST

namespace power {
float read_battery_voltage() {
    return 3.85f;
}
} // namespace power

#else

#if HAS_M5_POWER
#include <M5Unified.h>
#endif

#if defined(BOARD_ESP32WROOM_6050)
#include <Arduino.h>
#endif

namespace power {

float read_battery_voltage() {
#if HAS_M5_POWER
    // M5.Power returns millivolts
    return static_cast<float>(M5.Power.getBatteryVoltage()) / 1000.0f;
#elif defined(BAT_ADC_PIN)
    // 1:1 voltage divider on production board
    return static_cast<float>(analogReadMilliVolts(BAT_ADC_PIN)) * 2.0f / 1000.0f;
#else
    return 0.0f;
#endif
}

} // namespace power

#endif // HOST_TEST
