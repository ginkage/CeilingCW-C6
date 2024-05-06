#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

static const char *TAG = "ESP_ZB_CEILING_LIGHT";

#define HA_ESP_LIGHT_ENDPOINT           10      /* esp light bulb device endpoint, used to process light controlling commands */

void light_init(void);

void light_set_on_off(bool power);

void light_set_startup_on_off(uint8_t startup);

void light_set_level(uint8_t level);

void light_set_temperature(uint16_t temperature);

void light_set_startup_temperature(uint16_t startup);

void light_set_defaults();

void light_load_settings();

#ifdef __cplusplus
} // extern "C"
#endif
