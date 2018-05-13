#include "wordclock.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event_loop.h>
#include <esp_attr.h>
#include <esp_sleep.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <time.h>
#include <sys/time.h>

#include <nvs_flash.h>

#include <lwip/err.h>

#include <apps/sntp/sntp.h>

#include <string.h>
#include <stdbool.h>

#include "esp32_digital_led_lib.h"

#include "http_server.h"
#include "wifi_manager.h"

// define LED strands
strand_t STRANDS[] = {
    {.rmtChannel = 0,
     .gpioNum = 13,
     .ledType = LED_WS2812B_V3,
     .brightLimit = 32,
     .numPixels = 1,
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

void apply_mask(strand_t *pStrand, bool mask[],
                pixelColor_t colorOn, pixelColor_t colorOff) {
    for (int i=0; i < pStrand->numPixels; i++) {
        if (mask[i]) {
            pStrand->pixels[i] = colorOn;
        }
        else {
            pStrand->pixels[i] = colorOff;
        }
    }
}

/* Pull the current time from pool.ntp.org
 */
static void init_sntp(void) {
    ESP_LOGI("init_sntp", "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

/* Run the wordclock logic
 *
 * This function takes the current time and determins how it should be displayed
 * using our defined ws2812b LED strand
 */
void run_wordclock(void *pvParameter)
{
    ESP_LOGI("run_wordclock", "Initializing %d strand(s)...", STRANDCNT);
    digitalLeds_initStrands(STRANDS, STRANDCNT);
    strand_t *pStrand = &STRANDS[0];
    pixelColor_t colorOn = pixelFromRGB(100, 100, 100);
    pixelColor_t colorOff = pixelFromRGB(0, 0, 0);

    time_t now;
    struct tm *timeinfo;
    time(&now);
    timeinfo = localtime(&now);
    while (timeinfo->tm_year < (2016 - 1900)) {
            // System time is not yet set, another task takes care of that so we just wait here
            ESP_LOGI("run_wordclock", "Time is not set yet... Waiting");
            // update 'now' variable with current time
            time(&now);
            timeinfo = localtime(&now);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    while (true) {
        log_time();
        pStrand->pixels[0] = colorOn;
        digitalLeds_updatePixels(pStrand);
        vTaskDelay(2500 / portTICK_PERIOD_MS);
        pStrand->pixels[0] = colorOff;
        digitalLeds_updatePixels(pStrand);
        vTaskDelay(2500 / portTICK_PERIOD_MS);
    }
}

/* Helper function to refresh the system time by connecting to wifi and
 * getting the current time via ntp
 */
static void refresh_time(void)
{
    ESP_LOGI("refresh_time", "Starting refresh_time()")
    init_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 100;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI("refresh_time", "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }

    ESP_ERROR_CHECK( esp_wifi_stop() );
}

/* Helper function to log the current time via ESP_LOGI in a human readable
 * form
 */
static void log_time()
{
    time_t now;
    time(&now);
    struct tm *timeinfo;
    timeinfo = localtime(&now);
    ESP_LOGI("time", "%s", asctime(timeinfo));
}

/* Main entry point of the app. Start all necessary tasks as separate
 * FreeRTOS tasks so they run in parallel
 */
void app_main() {
    ESP_LOGI("app_main", "*********************");
    ESP_LOGI("app_main", "* starting app_main *");
    ESP_LOGI("app_main", "*********************");

    ESP_ERROR_CHECK( nvs_flash_init() );

    setenv("TZ", "WEST-1DWEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
    tzset();

    xTaskCreate(&http_server, "http_server", 2048, NULL, 5, NULL);
    xTaskCreate(&wifi_manager, "wifi_manager", 4096, NULL, 4, NULL);

    xTaskCreate(&run_wordclock, "run_wordclock", 4096, NULL, 5, NULL);

    refresh_time();
}
