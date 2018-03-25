#include "main.h"

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

#include "esp32_digital_led_lib.h"

#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD


// define LED strands
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

// event group to signal wifi connection and readiness
static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

void print_mask(bool mask[], int len) {
    for (int i = 0; i < len; i++) {
        ets_printf("%d ", mask[i]);
    }
    ets_printf("\n");
}

void apply_mask(strand_t *pStrand, bool mask[],
                pixelColor_t colorOn, pixelColor_t colorOff) {
    for (int i=0; i < pStrand->numPixels; i++) {
        mask[i] ? pStrand->pixels[i] = colorOn :
                pStrand->pixels[i] = colorOff;
    }
}

static void refresh_time(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    init_wifi();
    xEventGroupWaitBits(wifi_event_group,
                        CONNECTED_BIT,
                        false,
                        true,
                        portMAX_DELAY);
    init_sntp();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int max_retry = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < max_retry) {
        ESP_LOGI("refresh_time",
                 "Waiting for system time to be set... (%d/%d)",
                 retry, max_retry);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
}

static void init_wifi(void) {
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        // TODO: Not allowed in c++.
        //		.sta = {
        //			.ssid = WIFI_SSID,
        //			.password = WIFI_PASS,
        //		},
    };
    ESP_LOGI("init_wifi", "Setting up wifi for SSID %s...",
             wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void init_sntp(void) {
    ESP_LOGI("init_sntp", "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static esp_err_t event_handler(void *ctx, system_event_t *event) {
    switch (event->event_id){
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

extern "C" {
int app_main();
}

int app_main() {
    ESP_LOGI("app_main", "*********************");
    ESP_LOGI("app_main", "* starting app_main *");
    ESP_LOGI("app_main", "*********************");

    ESP_LOGI("app_main", "Initializing %d strand(s)...", STRANDCNT);
    digitalLeds_initStrands(STRANDS, STRANDCNT);

    ESP_LOGI("app_main", "Getting current time...");
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // check if time is set correctly
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI("app_main", "Time not set correctly. Getting time over NTP...");
        ESP_LOGI("app_main", "%d", timeinfo.tm_year);
        //		refresh_time();
    }
    ESP_LOGI("app_main", "Done setting time...");

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
