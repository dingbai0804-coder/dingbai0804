#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t CST816_Init(void);
bool CST816_Read(uint16_t *x, uint16_t *y);
