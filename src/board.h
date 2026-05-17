#ifndef BOARD_H
#define BOARD_H

// Board selection — exactly one BOARD_* must be defined by PlatformIO build flags.
// HOST_TEST builds define none; imu/power/flash use mock paths.

#if defined(BOARD_M5STICKC_6886)
    #define IMU_BACKEND_MPU6886 1
    #define HAS_LCD 1
    #define HAS_M5_POWER 1
    #define HAS_FLASH_BUFFER 0
    #define BOARD_NAME "M5StickC-6886"

#elif defined(BOARD_M5STICKC_6050)
    #define IMU_BACKEND_MPU6050 1
    #define I2C_SDA 32
    #define I2C_SCL 33
    #define MPU6050_I2C_ADDR 0x68
    #define HAS_LCD 1
    #define HAS_M5_POWER 1
    #define HAS_FLASH_BUFFER 0
    #define BOARD_NAME "M5StickC-6050"

#elif defined(BOARD_ESP32WROOM_6050)
    #define IMU_BACKEND_MPU6050 1
    #define I2C_SDA 21
    #define I2C_SCL 22
    #define MPU6050_I2C_ADDR 0x68
    #define HAS_LCD 0
    #define HAS_M5_POWER 0
    #define HAS_FLASH_BUFFER 1
    #define BAT_ADC_PIN 34
    #define FLASH_CS_PIN 5
    #define FLASH_SPI_MOSI 23
    #define FLASH_SPI_MISO 19
    #define FLASH_SPI_SCK 18
    #define BOARD_NAME "ESP32WROOM-6050"

#else
    // Host tests or unspecified — no hardware backends
    #define HAS_LCD 0
    #define HAS_M5_POWER 0
    #define HAS_FLASH_BUFFER 0
    #define BOARD_NAME "HOST"
#endif

#endif // BOARD_H
