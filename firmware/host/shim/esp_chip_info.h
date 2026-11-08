#pragma once

#include <stdint.h>

typedef enum { CHIP_ESP32 = 1, CHIP_ESP32S2 = 2, CHIP_ESP32C3 = 5, CHIP_ESP32S3 = 9 } esp_chip_model_t;

constexpr uint32_t CHIP_FEATURE_WIFI_BGN = 1u << 0;
constexpr uint32_t CHIP_FEATURE_BLE = 1u << 4;
constexpr uint32_t CHIP_FEATURE_BT = 1u << 5;

typedef struct {
  esp_chip_model_t model;
  uint32_t features;
  uint8_t cores;
  uint8_t revision;
} esp_chip_info_t;

void esp_chip_info(esp_chip_info_t* info);
