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

void app_main() {
	ets_printf("Initializing strands...\n");
	digitalLeds_initStrands(STRANDS, STRANDCNT);

	// define the on and off color for the leds and get a pointer to the
	// strand we want to work with
	pixelColor_t color1 = pixelFromRGB(100, 100, 100);
	pixelColor_t colorOff = pixelFromRGB(0, 0, 0);
	strand_t * pStrand = &STRANDS[0];
	while (true) {
		for (int i = 0; i < pStrand->numPixels; i++) {
			// TODO: this always leaves the last LED on
			pStrand->pixels[i] = color1;
			pStrand->pixels[i-1] = colorOff;
			digitalLeds_updatePixels(pStrand);
			vTaskDelay(100 / portTICK_PERIOD_MS);
		}
	}
}
