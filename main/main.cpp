#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp32_digital_led_lib.h"

strand_t STRANDS[] = {
	{.rmtChannel = 0,
	 .gpioNum = 17,
	 .ledType = LED_WS2812B_V3,
	 .brightLimit = 32,
	 .numPixels = 20,
	 .pixels = NULL,
	 ._stateVars = NULL},
};

int STRANDCNT = sizeof(STRANDS) / sizeof(STRANDS[0]);

void print_mask(bool mask[], int len) {
	for (int i = 0; i < len; i++) {
		ets_printf("%d ", mask[i]);
	}
	ets_printf("\n");
}

extern "C" {
	int app_main();
}

void apply_mask(strand_t *pStrand, bool mask[],
		pixelColor_t colorOn, pixelColor_t colorOff) {
	for (int i=0; i < pStrand->numPixels; i++) {
		mask[i] ? pStrand->pixels[i] = colorOn :
			  pStrand->pixels[i] = colorOff;
	}
}

int app_main() {
	ESP_LOGI("app_main", "*********************");
	ESP_LOGI("app_main", "* starting app_main *");
	ESP_LOGI("app_main", "*********************");

	ESP_LOGI("app_main", "Initializing %d strand(s)...", STRANDCNT);
	digitalLeds_initStrands(STRANDS, STRANDCNT);

	// define the on and off color for the leds and get a pointer to the
	// strand we want to work with
	pixelColor_t colorOn = pixelFromRGB(50, 50, 50);
	pixelColor_t colorOff = pixelFromRGB(0, 0, 0);
	strand_t * pStrand = &STRANDS[0];

	// Try and set some LEDs on by using a mask
	bool mask[pStrand->numPixels] = { 0, 1, 1, 1, 0, 1, 1 };
	int len = sizeof(mask) / sizeof(mask[0]);
	print_mask(mask, len);

	while (true) {
		apply_mask(pStrand, mask, colorOn, colorOff);
		digitalLeds_updatePixels(pStrand);
		vTaskDelay(500 / portTICK_PERIOD_MS);
		//for (int i=0; i < pStrand->numPixels; i++) {
		//	mask[i] = !mask[i];
		//}
	}
}

