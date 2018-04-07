#ifndef WORDCLOCK_H
#define WORDCLOCK_H

#include <esp_err.h>
#include <esp_event.h>
#include <time.h>

#include "esp32_digital_led_lib.h"

void apply_mask(strand_t *pStrand, bool mask[], pixelColor_t colorOn, pixelColor_t colorOff);
static void init_sntp(void);

static void log_time(void);
static void refresh_time(void);
void continuous_refresh_time(void *pvParameter);

void run_wordclock(void *pvParameter);

#endif // WORDCLOCK_H
