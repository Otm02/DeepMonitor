#include "imu.h"
#include "board.h"
#include "config.h"
#include <cmath>

#ifdef HOST_TEST
// ── Mock implementation for host-side testing ───────────────────────────────

namespace {
    bool mock_imu_initialized = false;
    bool mock_imu_should_fail = false;
}

namespace imu {
namespace mock {
    void set_should_fail(bool fail) {
        mock_imu_should_fail = fail;
    }

    void reset() {
        mock_imu_initialized = false;
        mock_imu_should_fail = false;
    }
} // namespace mock

bool init() {
    if (mock_imu_should_fail) {
        return false;
    }
    mock_imu_initialized = true;
    return true;
}

void sample(float vib_out[config::SAMPLE_COUNT], float temp_out[config::SAMPLE_COUNT]) {
    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        float ax = 0.001f * static_cast<float>(i);
        float ay = 0.002f * static_cast<float>(i);
        float az = 0.003f * static_cast<float>(i);
        vib_out[i] = std::sqrt(ax * ax + ay * ay + az * az);
        temp_out[i] = 25.0f + 0.01f * static_cast<float>(i);
    }
}

} // namespace imu

#else
// ── Hardware implementations ──────────────────────────────────────────────────

#if defined(IMU_BACKEND_MPU6886)

#include <M5Unified.h>

namespace imu {

bool init() {
    auto result = M5.Imu.init();
    return (result != 0);
}

void sample(float vib_out[config::SAMPLE_COUNT], float temp_out[config::SAMPLE_COUNT]) {
    const unsigned long sample_interval_us =
        1000000UL / static_cast<unsigned long>(config::SAMPLE_RATE_HZ);

    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        unsigned long start_us = micros();

        M5.Imu.update();

        float ax = 0.0f, ay = 0.0f, az = 0.0f;
        M5.Imu.getAccelData(&ax, &ay, &az);

        float temp = 25.0f;
        M5.Imu.getTemp(&temp);

        vib_out[i] = std::sqrt(ax * ax + ay * ay + az * az);
        temp_out[i] = temp;

        unsigned long elapsed = micros() - start_us;
        if (elapsed < sample_interval_us) {
            delayMicroseconds(sample_interval_us - elapsed);
        }
    }
}

} // namespace imu

#elif defined(IMU_BACKEND_MPU6050)

#include <Wire.h>
#include <Arduino.h>

namespace {
    static const uint8_t REG_WHO_AM_I   = 0x75;
    static const uint8_t REG_PWR_MGMT_1 = 0x6B;
    static const uint8_t REG_ACCEL_XOUT_H = 0x3B;
    static const uint8_t REG_TEMP_OUT_H   = 0x41;
    static const uint8_t WHO_AM_I_EXPECT  = 0x68;

    uint8_t s_i2c_addr = MPU6050_I2C_ADDR;

    bool mpu6050_write_byte(uint8_t reg, uint8_t val) {
        Wire.beginTransmission(s_i2c_addr);
        Wire.write(reg);
        Wire.write(val);
        return Wire.endTransmission() == 0;
    }

    bool mpu6050_read_bytes(uint8_t reg, uint8_t* buf, size_t len) {
        Wire.beginTransmission(s_i2c_addr);
        Wire.write(reg);
        if (Wire.endTransmission(false) != 0) {
            return false;
        }
        size_t got = Wire.requestFrom(static_cast<uint8_t>(s_i2c_addr), static_cast<uint8_t>(len));
        if (got != len) {
            return false;
        }
        for (size_t i = 0; i < len; i++) {
            buf[i] = Wire.read();
        }
        return true;
    }

    bool mpu6050_probe() {
        uint8_t who = 0;
        if (!mpu6050_read_bytes(REG_WHO_AM_I, &who, 1)) {
            return false;
        }
        if (who == WHO_AM_I_EXPECT) {
            s_i2c_addr = MPU6050_I2C_ADDR;
            return true;
        }
        // AD0 high → address 0x69
        s_i2c_addr = 0x69;
        if (!mpu6050_read_bytes(REG_WHO_AM_I, &who, 1)) {
            return false;
        }
        return who == WHO_AM_I_EXPECT;
    }
}

namespace imu {

bool init() {
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);

    if (!mpu6050_probe()) {
        return false;
    }

    // Wake MPU-6050 from sleep
    if (!mpu6050_write_byte(REG_PWR_MGMT_1, 0x00)) {
        return false;
    }
    delay(50);
    return true;
}

void sample(float vib_out[config::SAMPLE_COUNT], float temp_out[config::SAMPLE_COUNT]) {
    const unsigned long sample_interval_us =
        1000000UL / static_cast<unsigned long>(config::SAMPLE_RATE_HZ);

    for (int i = 0; i < config::SAMPLE_COUNT; ++i) {
        unsigned long start_us = micros();

        uint8_t raw[14];
        if (!mpu6050_read_bytes(REG_ACCEL_XOUT_H, raw, 14)) {
            vib_out[i] = 0.0f;
            temp_out[i] = 25.0f;
        } else {
            int16_t ax = static_cast<int16_t>((raw[0] << 8) | raw[1]);
            int16_t ay = static_cast<int16_t>((raw[2] << 8) | raw[3]);
            int16_t az = static_cast<int16_t>((raw[4] << 8) | raw[5]);
            int16_t temp_raw = static_cast<int16_t>((raw[6] << 8) | raw[7]);

            // Default ±2g → 16384 LSB/g; temp: T°C = raw/340 + 36.53
            float fax = static_cast<float>(ax) / 16384.0f;
            float fay = static_cast<float>(ay) / 16384.0f;
            float faz = static_cast<float>(az) / 16384.0f;
            vib_out[i] = std::sqrt(fax * fax + fay * fay + faz * faz);
            temp_out[i] = static_cast<float>(temp_raw) / 340.0f + 36.53f;
        }

        unsigned long elapsed = micros() - start_us;
        if (elapsed < sample_interval_us) {
            delayMicroseconds(sample_interval_us - elapsed);
        }
    }
}

} // namespace imu

#else
#error "No IMU backend defined — set BOARD_* build flag"
#endif

#endif // HOST_TEST
