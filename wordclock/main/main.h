#ifndef MAIN_H
#define MAIN_H

#include <esp_err.h>
#include <esp_event.h>

#include "esp32_digital_led_lib.h"

void apply_mask(strand_t *pStrand, bool mask[], pixelColor_t colorOn, pixelColor_t colorOff);
static void refresh_time(void);
static void init_wifi(void);
static void init_sntp(void);
static esp_err_t event_handler(void *ctx, system_event_t *event);

#endif // MAIN_H
