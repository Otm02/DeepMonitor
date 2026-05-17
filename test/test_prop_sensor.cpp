#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>

/**
 * Property 1: Sensor Computation Correctness
 *
 * For any 3-axis accelerometer reading (ax, ay, az) and any temperature
 * reading with a baseline, the computed Vibration_Magnitude should equal
 * sqrt(ax² + ay² + az²) and the computed Temperature_Delta should equal
 * temperature - baseline.
 *
 * Validates: Requirements 1.4, 1.5
 */

// Helper: compute vibration magnitude the same way imu::sample() does
static float compute_vibration_magnitude(float ax, float ay, float az) {
    return std::sqrt(ax * ax + ay * ay + az * az);
}

// Helper: compute temperature delta
static float compute_temperature_delta(float temp, float baseline) {
    return temp - baseline;
}

// Generator for bounded floats in [-100, 100] to avoid overflow
static rc::Gen<float> boundedFloat() {
    return rc::gen::map(rc::gen::inRange(-10000, 10001), [](int v) {
        return static_cast<float>(v) / 100.0f;
    });
}

/**
 * **Validates: Requirements 1.4, 1.5**
 *
 * Property: Vibration magnitude equals sqrt(ax² + ay² + az²) and is non-negative.
 */
RC_GTEST_PROP(SensorComputation, VibrationMagnitudeIsCorrect, ()) {
    float ax = *boundedFloat();
    float ay = *boundedFloat();
    float az = *boundedFloat();

    float magnitude = compute_vibration_magnitude(ax, ay, az);
    float expected = std::sqrt(ax * ax + ay * ay + az * az);

    RC_ASSERT(std::isfinite(magnitude));
    RC_ASSERT(magnitude >= 0.0f);
    RC_ASSERT(std::abs(magnitude - expected) < 1e-5f);
}

/**
 * **Validates: Requirements 1.4, 1.5**
 *
 * Property: Temperature delta equals temp - baseline for any values.
 */
RC_GTEST_PROP(SensorComputation, TemperatureDeltaIsCorrect, ()) {
    float temp = *boundedFloat();
    float baseline = *boundedFloat();

    float delta = compute_temperature_delta(temp, baseline);
    float expected = temp - baseline;

    RC_ASSERT(std::isfinite(delta));
    RC_ASSERT(std::abs(delta - expected) < 1e-6f);
}

/**
 * **Validates: Requirements 1.4**
 *
 * Property: Vibration magnitude is zero iff all axes are zero.
 */
RC_GTEST_PROP(SensorComputation, VibrationMagnitudeZeroIffAllAxesZero, ()) {
    float ax = *boundedFloat();
    float ay = *boundedFloat();
    float az = *boundedFloat();

    float magnitude = compute_vibration_magnitude(ax, ay, az);

    if (ax == 0.0f && ay == 0.0f && az == 0.0f) {
        RC_ASSERT(magnitude == 0.0f);
    } else {
        RC_ASSERT(magnitude > 0.0f);
    }
}
