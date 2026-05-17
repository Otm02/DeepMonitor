# Implementation Plan: Predictive Maintenance Sensor Firmware

## Overview

Implement the predictive maintenance sensor firmware for the M5StickC Plus 2 in C++ using namespace-based modular design with static memory allocation. Tasks are organized in four phases matching the design: Sensor Fusion & Preprocessing, ML Engine, Deep Sleep & RTC, and NVS & Connectivity. All pure computation modules are tested on the host using Google Test and RapidCheck. Hardware-dependent modules use compile-time mocking (`#ifdef HOST_TEST`).

## Tasks

- [x] 1. Project scaffolding and configuration constants
  - [x] 1.1 Create project directory structure and CMakeLists.txt for host-side testing with Google Test and RapidCheck
    - Set up `src/` for firmware source, `test/` for host-side tests
    - Configure CMake to build host-side test binary linking Google Test and RapidCheck
    - _Requirements: 10.1, 10.2_

  - [x] 1.2 Define `config` namespace with all configuration constants
    - Create `src/config.h` with all constants from the design (SMOOTHING_ALPHA, ANOMALY_THRESHOLD, LR_INITIAL, LR_DECAY_RATE, SAMPLE_COUNT, WINDOW_SIZE, INPUT_DIM, LATENT_DIM, etc.)
    - _Requirements: 2.2, 4.6, 5.1, 7.1, 9.1_

  - [x] 1.3 Define all struct types and namespace interfaces in header files
    - Create `src/imu.h`, `src/normalizer.h`, `src/window.h`, `src/ml.h`, `src/rtc.h`, `src/nvs.h`, `src/connectivity.h`
    - Declare structs: `normalizer::Stats`, `window::WindowState`, `ml::Weights`, `ml::InferenceResult`, `rtc::RtcBlock`, `connectivity::AlertPayload`, `connectivity::HeartbeatPayload`
    - Declare function signatures per the design interfaces
    - _Requirements: 10.1, 10.2_

- [x] 2. Phase 1 — Sensor Fusion & Preprocessing
  - [x] 2.1 Implement `imu::init()` and `imu::sample()`
    - `init()` calls `M5.Imu.Init()`, returns false on failure, must complete within 500ms
    - `sample()` reads accelerometer and temperature at 100Hz for 500 samples
    - Computes `Vibration_Magnitude = sqrt(ax² + ay² + az²)` per sample
    - Computes `Temperature_Delta = temp - baseline` per sample
    - Use `#ifdef HOST_TEST` to mock I2C/IMU calls for host-side testing
    - _Requirements: 1.1, 1.2, 1.3, 1.4, 1.5_

  - [x] 2.2 Write property test for sensor computation correctness
    - **Property 1: Sensor Computation Correctness**
    - Generate random (ax, ay, az, temp, baseline) tuples, verify `sqrt(ax² + ay² + az²)` and `temp - baseline`
    - **Validates: Requirements 1.4, 1.5**

  - [x] 2.3 Implement `normalizer::update()`, `normalizer::normalize()`, and `normalizer::freeze()`
    - EMA update: `mean_new = (1 - α) * mean_old + α * batch_mean`
    - Variance EMA: `var_new = (1 - α) * var_old + α * batch_var`
    - Normalization: `x_norm = (x - mean) / sqrt(var + 1e-6)`
    - `freeze()` skips update when anomaly detected
    - _Requirements: 2.1, 2.2, 2.3, 2.4_

  - [x] 2.4 Write property test for EMA update formula
    - **Property 2: EMA Update Formula**
    - Generate random (old_mean, old_var, batch samples, α), verify update formula
    - **Validates: Requirements 2.2**

  - [x] 2.5 Write property test for normalization output
    - **Property 3: Normalization Produces Zero-Mean Unit-Variance Output**
    - Generate random batches with positive variance stats, verify normalized output statistics within ε
    - **Validates: Requirements 2.3**

  - [x] 2.6 Write property test for normalizer freeze on anomaly
    - **Property 4: Normalizer Freezes on Anomaly**
    - Generate random stats and batches, verify stats unchanged when anomaly flag is set
    - **Validates: Requirements 2.4**

- [x] 3. Checkpoint — Phase 1
  - Ensure all tests pass, ask the user if questions arise.

- [x] 4. Phase 2 — ML Engine
  - [x] 4.1 Implement `window::append()` and `window::to_flat_array()`
    - Circular buffer of `WINDOW_SIZE` (16) entries, each with mean_vib and mean_temp
    - `append()` overwrites oldest when full
    - `to_flat_array()` produces temporal-order flat array of size `INPUT_DIM` (32)
    - _Requirements: 3.1, 3.2, 3.4_

  - [x] 4.2 Write property test for circular buffer
    - **Property 5: Circular Buffer Maintains Recent Entries**
    - Generate random sequences of entries, verify buffer contains most recent `min(N, WINDOW_SIZE)` entries in order
    - **Validates: Requirements 3.1, 3.2, 3.4**

  - [x] 4.3 Implement `ml::infer()` — encoder, decoder, reconstruction error, anomaly classification
    - Encoder: `latent[j] = Σ_i encoder[j][i] * input[i]`
    - Decoder: `reconstructed[i] = Σ_j decoder[i][j] * latent[j]`
    - MSE: `(1/INPUT_DIM) * Σ_i (input[i] - reconstructed[i])²`
    - `is_anomaly = (mse > threshold)`
    - All computation uses static buffers, no heap allocation
    - _Requirements: 4.1, 4.3, 4.4, 10.1_

  - [x] 4.4 Write property test for anomaly classification
    - **Property 7: Anomaly Classification Matches MSE vs Threshold**
    - Generate random inputs and weights, verify `is_anomaly` matches `MSE > threshold`
    - **Validates: Requirements 4.3, 4.4**

  - [x] 4.5 Implement `ml::update_weights()` using Sanger's Rule and `ml::init_weights_random()`
    - Sanger's Rule: `Δw_ji = η * (latent[j] * input[i] - Σ_{k≤j} latent[k] * w_ki * latent[j])`
    - Decoder update via tied weights (transpose of encoder update)
    - Skip update when `is_anomaly == true`
    - Learning rate decay: `lr = lr_initial / (1 + decay_rate * normal_cycle_count)`
    - Learning rate reset after N consecutive anomalous cycles
    - `init_weights_random()` seeds from ESP32 hardware RNG (mocked for host tests)
    - _Requirements: 4.2, 4.5, 4.6, 4.8_

  - [x] 4.6 Write property test for weight update conditioned on anomaly
    - **Property 6: Weight Update Conditioned on Anomaly Status**
    - Generate random inputs and weights, verify weights change iff cycle is normal
    - **Validates: Requirements 4.2, 4.5**

  - [x] 4.7 Write property test for learning rate decay and reset
    - **Property 8: Learning Rate Decay and Reset**
    - Generate random sequences of normal/anomalous cycles, verify LR decay formula and reset behavior
    - **Validates: Requirements 4.6**

  - [x] 4.8 Implement `ml::compute_weight_shift()` for NVS save trigger
    - Compute L2 norm of difference between current and saved weight matrices
    - _Requirements: 7.2_

- [x] 5. Checkpoint — Phase 2
  - Ensure all tests pass, ask the user if questions arise.

- [x] 6. Phase 3 — Deep Sleep & RTC Memory
  - [x] 6.1 Implement `rtc::compute_crc16()`, `rtc::save()`, and `rtc::validate()`
    - CRC-16 computed over all RtcBlock fields except the `crc16` field itself
    - `save()` computes CRC and stores it in the block
    - `validate()` recomputes CRC and compares
    - Declare `RTC_DATA_ATTR RtcBlock rtc_block;` (guarded by `#ifndef HOST_TEST`)
    - _Requirements: 6.1, 6.3, 6.4_

  - [x] 6.2 Write property test for CRC-16 round-trip validation
    - **Property 9: CRC-16 Round-Trip Validation**
    - Generate random RtcBlock data, verify CRC validates; flip random bits, verify CRC fails
    - **Validates: Requirements 6.3**

  - [x] 6.3 Implement RTC state restore logic on wake
    - On wake: call `rtc::validate()`, if valid restore state from `rtc_block`
    - If invalid: treat as Hard_Boot, attempt NVS restore
    - _Requirements: 6.2, 6.4_

  - [x] 6.4 Implement deep sleep entry with configurable interval
    - Call `rtc::save()` before sleep to persist all state with CRC
    - Use `esp_deep_sleep(config::SLEEP_DURATION_US)` (mocked for host tests)
    - Ensure Wi-Fi, Bluetooth, and LCD are disabled before sleep
    - _Requirements: 5.1, 5.3_

  - [x] 6.5 Add static assertion for RTC memory budget
    - `static_assert(sizeof(rtc::RtcBlock) <= 8192, "RTC memory budget exceeded")`
    - _Requirements: 10.3_

- [x] 7. Checkpoint — Phase 3
  - Ensure all tests pass, ask the user if questions arise.

- [x] 8. Phase 4 — NVS Persistence & Connectivity
  - [x] 8.1 Implement `nvs::save()`, `nvs::load()`, and `nvs::has_valid_data()`
    - Use `Preferences.h` library (mocked for host tests)
    - Write weights as single blob via `putBytes`, stats as second blob
    - `has_valid_data()` checks if blobs exist and are the expected size
    - _Requirements: 7.3, 7.4, 7.5, 7.6_

  - [x] 8.2 Write property test for NVS persistence round trip
    - **Property 12: NVS Persistence Round Trip**
    - Generate random weights and stats, save then load via mocked Preferences, verify equality
    - **Validates: Requirements 7.4**

  - [x] 8.3 Implement NVS save trigger logic (time-based and weight-shift)
    - Time-based: trigger when `(duty_cycle_count - last_nvs_save_cycle) >= NVS_SAVE_INTERVAL`
    - Weight-shift: trigger when `compute_weight_shift(current, saved) >= WEIGHT_SHIFT_THRESHOLD`
    - _Requirements: 7.1, 7.2_

  - [x] 8.4 Write property test for NVS time-based save trigger
    - **Property 10: NVS Time-Based Save Trigger**
    - Generate random (cycle_count, last_save_cycle) pairs, verify trigger condition
    - **Validates: Requirements 7.1**

  - [x] 8.5 Write property test for NVS weight-shift save trigger
    - **Property 11: NVS Weight-Shift Save Trigger**
    - Generate random weight pairs, verify trigger matches threshold comparison
    - **Validates: Requirements 7.2**

  - [x] 8.6 Implement `connectivity::connect_wifi()`, `connectivity::disconnect_wifi()`
    - 10-second timeout, increment `wifi_fail_count` in RTC on failure
    - After transmission: `WiFi.disconnect(true); WiFi.mode(WIFI_OFF);`
    - Use `#ifdef HOST_TEST` to mock Wi-Fi stack
    - _Requirements: 8.1, 8.3, 8.5, 9.4_

  - [x] 8.7 Implement `connectivity::send_alert()` with JSON serialization
    - Serialize AlertPayload to JSON using `snprintf` into a static char buffer (no ArduinoJson)
    - HTTP POST via `HTTPClient` (mocked for host tests)
    - On non-2xx response: increment `http_retry_count` in RTC
    - _Requirements: 8.2, 8.4_

  - [x] 8.8 Write property test for alert payload fields
    - **Property 13: Alert Payload Contains All Required Fields**
    - Generate random AlertPayload values, serialize, verify all required JSON keys present
    - **Validates: Requirements 8.2**

  - [x] 8.9 Write property test for HTTP retry counter
    - **Property 14: HTTP Retry Counter Increments on Failure**
    - Generate random initial retry counts, simulate failure, verify increment
    - **Validates: Requirements 8.4**

  - [x] 8.10 Implement `connectivity::send_heartbeat()` with JSON serialization
    - Serialize HeartbeatPayload to JSON using `snprintf` into a static char buffer
    - HTTP POST via `HTTPClient` (mocked for host tests)
    - _Requirements: 9.2, 9.3_

  - [x] 8.11 Implement heartbeat trigger logic
    - Trigger when `(duty_cycle_count - last_heartbeat_cycle) >= HEARTBEAT_INTERVAL`
    - _Requirements: 9.1_

  - [x] 8.12 Write property test for heartbeat interval trigger
    - **Property 15: Heartbeat Interval Trigger**
    - Generate random (cycle_count, last_heartbeat_cycle) pairs, verify trigger condition
    - **Validates: Requirements 9.1**

  - [x] 8.13 Write property test for heartbeat payload fields
    - **Property 16: Heartbeat Payload Contains All Required Fields**
    - Generate random HeartbeatPayload values, serialize, verify all required JSON keys present
    - **Validates: Requirements 9.2**

- [x] 9. Checkpoint — Phase 4
  - Ensure all tests pass, ask the user if questions arise.

- [x] 10. Duty cycle orchestration and final integration
  - [x] 10.1 Implement the main `setup()` function orchestrating the full duty cycle
    - Wake → validate RTC → restore state → init IMU → sample → normalize → window → infer → anomaly/connectivity → NVS check → RTC save → deep sleep
    - Handle IMU failure (log to RTC, return to sleep)
    - Handle Hard_Boot path (NVS restore or fresh init)
    - Ensure Wi-Fi/BT/LCD disabled during sampling and inference
    - Complete sampling + inference within 8 seconds
    - _Requirements: 1.1, 1.2, 5.2, 5.3, 5.4, 6.2, 6.4_

  - [x] 10.2 Write unit tests for duty cycle orchestration
    - Test correct operation sequence with mocked modules
    - Test NVS save trigger conditions (time-based and weight-shift)
    - Test heartbeat trigger conditions
    - Test IMU failure path
    - Test Hard_Boot with valid NVS, Hard_Boot with no NVS
    - _Requirements: 5.2, 7.1, 7.2, 9.1_

- [x] 11. Final checkpoint — Ensure all tests pass
  - Ensure all tests pass, ask the user if questions arise.

## Notes

- Tasks marked with `*` are optional and can be skipped for faster MVP
- Each task references specific requirements for traceability
- All 16 correctness properties from the design are covered by property-based test tasks
- Property tests use RapidCheck on the host; unit tests use Google Test
- Hardware-dependent code is guarded with `#ifdef HOST_TEST` for mock substitution
- Checkpoints are placed after each phase for incremental validation
