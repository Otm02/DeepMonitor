# Requirements Document

## Introduction

This document specifies the requirements for a predictive maintenance (PdM) sensor firmware running on an M5StickC Plus 2 (ESP32-PICO-V3-02). The device is a retrofitted sensor for heavy machinery in a food processing plant. It monitors vibration and temperature from the onboard MPU6886 IMU to detect anomalies (bearing failures, overheating) before they cause unplanned downtime. The firmware uses an unsupervised online-learning autoencoder (Sanger's Rule) for anomaly detection, operates on a strict deep-sleep duty cycle to conserve the 200mAh battery, and connects to Wi-Fi only when an anomaly or scheduled heartbeat requires it.

Implementation is planned in four phases:
1. Sensor Fusion & Preprocessing
2. ML Engine (Online Temporal Autoencoder)
3. Deep Sleep & RTC Memory
4. NVS Persistence & Event-Driven Connectivity

## Glossary

- **Firmware**: The embedded C/C++ application running on the ESP32-PICO-V3-02 microcontroller.
- **IMU**: Inertial Measurement Unit — the MPU6886 sensor integrated in the M5StickC Plus 2, providing 3-axis accelerometer and gyroscope data plus a temperature reading.
- **Vibration_Magnitude**: The Euclidean magnitude of the 3-axis accelerometer vector: sqrt(ax² + ay² + az²).
- **Temperature_Delta**: The difference between the current IMU temperature reading and the Adaptive_Normalizer baseline temperature.
- **Adaptive_Normalizer**: A component that maintains running mean and variance estimates for Vibration_Magnitude and Temperature_Delta, used to normalize inputs to the ML_Engine.
- **ML_Engine**: The online temporal autoencoder neural network that learns a compressed representation of normal sensor behavior and flags deviations as anomalies.
- **Sanger_Rule**: The Generalized Hebbian Algorithm used to update the ML_Engine weights in an unsupervised, online fashion.
- **Reconstruction_Error**: The mean squared error between the ML_Engine input vector and its reconstructed output, used as the anomaly score.
- **Anomaly_Threshold**: A configurable Reconstruction_Error value above which the Firmware classifies the current reading as anomalous.
- **Sliding_Window**: A fixed-size circular buffer of recent normalized Vibration_Magnitude and Temperature_Delta samples used as input to the ML_Engine.
- **RTC_Memory**: The ESP32 RTC slow-memory region that retains data across deep-sleep cycles (RTC_DATA_ATTR).
- **NVS**: Non-Volatile Storage on the ESP32 flash, accessed via Preferences.h, used for long-term persistence of ML weights and normalizer baselines.
- **Duty_Cycle**: The repeating pattern of wake → sample → infer → sleep that governs Firmware operation.
- **Heartbeat**: A scheduled connectivity event (default every 12 hours) where the Firmware connects to Wi-Fi and transmits diagnostic data.
- **Webhook**: An HTTP endpoint that receives anomaly alerts and heartbeat payloads via HTTP POST.
- **Deep_Sleep**: The ESP32 low-power mode (esp_deep_sleep) used between Duty_Cycles to conserve battery.
- **Hard_Boot**: A full power-on reset (as opposed to waking from Deep_Sleep), typically caused by battery depletion or manual reset.
- **Learning_Rate**: The step-size parameter for Sanger_Rule weight updates, which controls how quickly the ML_Engine adapts to new data.

## Requirements

### Requirement 1: IMU Initialization and Sampling

**User Story:** As a maintenance engineer, I want the sensor to reliably read vibration and temperature data from the MPU6886, so that the ML engine has accurate inputs for anomaly detection.

#### Acceptance Criteria

1. WHEN the Firmware wakes from Deep_Sleep or performs a Hard_Boot, THE Firmware SHALL initialize the MPU6886 IMU within 500ms of wake.
2. IF the MPU6886 fails to respond during initialization, THEN THE Firmware SHALL log the failure to RTC_Memory and return to Deep_Sleep.
3. WHEN the IMU is initialized, THE Firmware SHALL sample the 3-axis accelerometer and temperature registers at 100Hz for 5 seconds, producing 500 sample sets.
4. THE Firmware SHALL compute Vibration_Magnitude for each sample as the Euclidean magnitude of the 3-axis accelerometer readings.
5. THE Firmware SHALL read the IMU temperature register once per sample and compute Temperature_Delta relative to the Adaptive_Normalizer baseline.

### Requirement 2: Adaptive Normalization

**User Story:** As a maintenance engineer, I want sensor readings to be normalized against a learned baseline, so that the ML engine receives consistent, scale-independent inputs regardless of seasonal or environmental drift.

#### Acceptance Criteria

1. THE Adaptive_Normalizer SHALL maintain a running mean and running variance for Vibration_Magnitude and Temperature_Delta.
2. WHEN a new batch of 500 samples is collected, THE Adaptive_Normalizer SHALL update the running mean and variance using an exponential moving average with a configurable smoothing factor.
3. THE Adaptive_Normalizer SHALL normalize each sample to zero mean and unit variance using the current running statistics before passing data to the ML_Engine.
4. WHEN the Reconstruction_Error exceeds the Anomaly_Threshold, THE Adaptive_Normalizer SHALL freeze updates to the running mean and variance for that Duty_Cycle.
5. THE Adaptive_Normalizer SHALL store the running mean and variance in RTC_Memory so that the statistics survive Deep_Sleep cycles.

### Requirement 3: Sliding Temporal Window

**User Story:** As a maintenance engineer, I want the ML engine to consider recent temporal context, so that it can detect patterns that develop over multiple duty cycles.

#### Acceptance Criteria

1. THE Firmware SHALL maintain a Sliding_Window as a fixed-size circular buffer of normalized Vibration_Magnitude and Temperature_Delta feature vectors.
2. WHEN a new batch of normalized samples is available, THE Firmware SHALL append the batch summary (mean Vibration_Magnitude, mean Temperature_Delta for the batch) to the Sliding_Window.
3. THE Sliding_Window SHALL store data in RTC_Memory so that temporal context survives Deep_Sleep cycles.
4. WHEN the Sliding_Window is full, THE Firmware SHALL overwrite the oldest entry with the newest entry.

### Requirement 4: Online Temporal Autoencoder (ML Engine)

**User Story:** As a maintenance engineer, I want the device to learn what "normal" machine behavior looks like without labeled data, so that it can detect deviations that indicate impending failures.

#### Acceptance Criteria

1. THE ML_Engine SHALL implement an autoencoder neural network with an encoder and decoder, where the encoder compresses the Sliding_Window input into a lower-dimensional latent representation and the decoder reconstructs the input from the latent representation.
2. THE ML_Engine SHALL update encoder and decoder weights using Sanger_Rule after each inference cycle.
3. THE ML_Engine SHALL compute Reconstruction_Error as the mean squared error between the Sliding_Window input and the reconstructed output.
4. WHEN the Reconstruction_Error exceeds the Anomaly_Threshold, THE ML_Engine SHALL classify the current Duty_Cycle as anomalous.
5. WHEN the Reconstruction_Error exceeds the Anomaly_Threshold, THE ML_Engine SHALL skip the Sanger_Rule weight update for that Duty_Cycle to prevent anomalous data from corrupting the learned baseline.
6. THE ML_Engine SHALL use a Learning_Rate that decays over successive normal Duty_Cycles to provide stability, and SHALL reset the Learning_Rate to a higher value after a configurable number of consecutive anomalous cycles to allow rapid re-adaptation.
7. THE ML_Engine SHALL store all weights in RTC_Memory so that the learned model survives Deep_Sleep cycles.
8. THE ML_Engine SHALL use only statically allocated memory (no heap allocation via `new`, `malloc`, or `String`) for all weight matrices and intermediate computation buffers.

### Requirement 5: Deep Sleep Duty Cycle

**User Story:** As a maintenance engineer, I want the device to conserve battery by sleeping between measurement cycles, so that the sensor can operate for extended periods on the 200mAh internal battery.

#### Acceptance Criteria

1. WHEN the Firmware completes inference and any required connectivity tasks, THE Firmware SHALL enter Deep_Sleep for a configurable interval (default 5 minutes).
2. WHEN the Firmware wakes from Deep_Sleep, THE Firmware SHALL execute a complete Duty_Cycle: initialize IMU, sample data, run Adaptive_Normalizer, update Sliding_Window, run ML_Engine inference, determine anomaly status, and handle connectivity if required.
3. THE Firmware SHALL NOT enable Wi-Fi, Bluetooth, or the LCD backlight during the sampling and inference phases of the Duty_Cycle.
4. THE Firmware SHALL complete the sampling and inference phases within 8 seconds of wake (excluding any connectivity time).

### Requirement 6: RTC Memory Management

**User Story:** As a maintenance engineer, I want the device to retain its learned model and sensor context across sleep cycles, so that every wake cycle benefits from prior learning without re-initialization.

#### Acceptance Criteria

1. THE Firmware SHALL store the following data in RTC_Memory using RTC_DATA_ATTR: ML_Engine weights, Adaptive_Normalizer running mean and variance, Sliding_Window contents, Duty_Cycle counter, and last NVS save timestamp.
2. WHEN the Firmware wakes from Deep_Sleep, THE Firmware SHALL read ML_Engine weights, Adaptive_Normalizer statistics, and Sliding_Window contents from RTC_Memory.
3. THE Firmware SHALL validate RTC_Memory contents on wake using a CRC-16 checksum stored alongside the data.
4. IF the RTC_Memory CRC-16 checksum validation fails, THEN THE Firmware SHALL treat the wake as a Hard_Boot and attempt to restore from NVS.

### Requirement 7: NVS Persistence

**User Story:** As a maintenance engineer, I want the device to save its learned model to flash memory periodically, so that the device recovers its learned baseline after a battery depletion or hard reset.

#### Acceptance Criteria

1. WHEN 24 hours have elapsed since the last NVS save (tracked via the Duty_Cycle counter), THE Firmware SHALL write the ML_Engine weights and Adaptive_Normalizer baselines to NVS.
2. WHEN the ML_Engine detects a cumulative weight shift exceeding a configurable threshold since the last NVS save, THE Firmware SHALL write the ML_Engine weights and Adaptive_Normalizer baselines to NVS.
3. WHEN the Firmware performs a Hard_Boot, THE Firmware SHALL check NVS for previously saved ML_Engine weights and Adaptive_Normalizer baselines.
4. IF valid NVS data exists on Hard_Boot, THEN THE Firmware SHALL restore ML_Engine weights and Adaptive_Normalizer baselines from NVS.
5. IF no valid NVS data exists on Hard_Boot, THEN THE Firmware SHALL initialize ML_Engine weights using small random values seeded from the ESP32 hardware random number generator and initialize Adaptive_Normalizer baselines to zero.
6. THE Firmware SHALL use the Preferences.h library to access NVS and SHALL write each data block as a single NVS blob to minimize flash wear.

### Requirement 8: Event-Driven Wi-Fi Anomaly Alerting

**User Story:** As a maintenance engineer, I want to receive an immediate alert when the sensor detects an anomaly, so that I can investigate the machine before a failure occurs.

#### Acceptance Criteria

1. WHEN the ML_Engine classifies a Duty_Cycle as anomalous, THE Firmware SHALL initialize the Wi-Fi stack and connect to a pre-configured SSID.
2. WHEN Wi-Fi is connected after an anomaly detection, THE Firmware SHALL send an HTTP POST to the configured Webhook URL containing: device ID, timestamp (millis since boot), Reconstruction_Error value, Anomaly_Threshold value, Vibration_Magnitude mean for the cycle, Temperature_Delta mean for the cycle, and battery voltage.
3. IF the Wi-Fi connection fails within 10 seconds, THEN THE Firmware SHALL abandon the connection attempt, log the failure count in RTC_Memory, and proceed to Deep_Sleep.
4. IF the HTTP POST fails or returns a non-2xx status code, THEN THE Firmware SHALL increment a retry counter in RTC_Memory and attempt to resend on the next anomalous Duty_Cycle.
5. WHEN the alert transmission completes (success or failure), THE Firmware SHALL disconnect Wi-Fi and disable the Wi-Fi stack before entering Deep_Sleep.

### Requirement 9: Heartbeat Connectivity

**User Story:** As a maintenance engineer, I want the device to periodically report its health and learned model state, so that I can monitor the sensor fleet remotely and verify devices are operational.

#### Acceptance Criteria

1. WHEN the Duty_Cycle counter indicates 12 hours have elapsed since the last Heartbeat, THE Firmware SHALL initialize the Wi-Fi stack and connect to the pre-configured SSID.
2. WHEN Wi-Fi is connected for a Heartbeat, THE Firmware SHALL send an HTTP POST to the configured Webhook URL containing: device ID, uptime (total Duty_Cycles since Hard_Boot), battery voltage, current Anomaly_Threshold, current Adaptive_Normalizer running mean and variance, and a summary of ML_Engine weight magnitudes.
3. IF the Wi-Fi connection or HTTP POST fails during a Heartbeat, THEN THE Firmware SHALL log the failure in RTC_Memory and retry on the next Heartbeat interval.
4. WHEN the Heartbeat transmission completes, THE Firmware SHALL disconnect Wi-Fi and disable the Wi-Fi stack before entering Deep_Sleep.

### Requirement 10: Static Memory Allocation

**User Story:** As a firmware developer, I want all runtime memory to be statically allocated, so that the device avoids heap fragmentation and operates reliably over weeks of continuous duty cycling.

#### Acceptance Criteria

1. THE Firmware SHALL NOT use dynamic memory allocation (malloc, calloc, realloc, new, or Arduino String) in the main loop or any function called during the Duty_Cycle.
2. THE Firmware SHALL declare all buffers, weight matrices, and working arrays as statically sized global or RTC_DATA_ATTR variables.
3. THE Firmware SHALL limit total RTC_Memory usage to 8KB (the ESP32 RTC slow memory capacity).
