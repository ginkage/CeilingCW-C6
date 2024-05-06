#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ledc_init(void);

void light_set_on_off(bool power);

#ifdef __cplusplus
} // extern "C"
#endif
