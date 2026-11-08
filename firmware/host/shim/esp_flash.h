#pragma once

#include <stdint.h>

typedef int esp_err_t;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;

struct esp_flash_t;
esp_err_t esp_flash_read_id(esp_flash_t* chip, uint32_t* out_id);
