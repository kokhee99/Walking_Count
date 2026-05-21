# Walking_Count

## Overview

This project implements a walking step counter using a Seeed XIAO ESP32S3 and the onboard 3-axis accelerometer accessed via the `LIS3DHTR` library. The OLED display is driven by the `U8g2` library (SSD1306 controller over I2C).

The step count is stored in the ESP32's non-volatile storage (NVS) so the count is preserved across power cycles. The code saves periodically and on each detected step (subject to debounce timing) to avoid data loss.

## Wiring (Seeed XIAO ESP32S3 Expansion Board)

Use the XIAO expansion board pins for I2C and power:

- `3V3` → `VCC` on MPU6050 and SSD1306
- `GND` → `GND` on MPU6050 and SSD1306
- `SDA` → `SDA` on MPU6050 and SSD1306
- `SCL` → `SCL` on MPU6050 and SSD1306

### Pin mapping with actual board labels

- XIAO `3V3` (power) → MPU6050 `VCC`, OLED `VCC`
- XIAO `GND` → MPU6050 `GND`, OLED `GND`
- XIAO `SDA` (I2C data, GPIO21) → MPU6050 `SDA`, OLED `SDA`
- XIAO `SCL` (I2C clock, GPIO22) → MPU6050 `SCL`, OLED `SCL`

### Wiring summary

```
Expansion Board    MPU6050      SSD1306 OLED
-----------        -------      ------------
3V3 ------------- VCC         VCC
GND ------------- GND         GND
SDA ------------- SDA         SDA
SCL ------------- SCL         SCL
```

### Notes

- The project uses the `LIS3DHTR` library to read the 3-axis accelerometer values (I2C).
- The OLED is driven using `U8g2` with the `SSD1306` controller over I2C.
- Both the accelerometer and the OLED share the same I2C bus on the XIAO expansion connector.
- The OLED reset pin is not required for this firmware (`U8X8_PIN_NONE` / no reset).
- Make sure all grounds are connected together.

## Build & Upload

1. Install PlatformIO (recommended via VS Code PlatformIO extension) or the PlatformIO Core CLI.
2. From the project root run:

```bash
# build the project
pio run

# to upload to a connected Seeed XIAO ESP32S3 (auto-detects port)
pio run --target upload
```

If PlatformIO is not on your PATH, use the VS Code PlatformIO extension to build/upload. The project `platformio.ini` includes the required libraries in `lib_deps` (`U8g2` and `LIS3DHTR`).
