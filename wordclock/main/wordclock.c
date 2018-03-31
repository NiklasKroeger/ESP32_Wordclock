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
        if (mask[i]) {
            pStrand->pixels[i] = colorOn;
        }
        else {
            pStrand->pixels[i] = colorOff;
        }
    }
}

/* Start our wifi so we can connect to our ntp server
 * TODO: This should probably be replaced by the convenient
 * WifiManager component for ESP32
 */
static void init_wifi(void) {
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_LOGI("init_wifi", "Setting up wifi for SSID %s...",
             wifi_config.sta.ssid);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

/* Pull the current time from pool.ntp.org
 */
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

/* Run the wordclock logic
 *
 * This function takes the current time and determins how it should be displayed
 * using our defined ws2812b LED strand
 */
void run_wordclock(void *pvParameter)
{
    ESP_LOGI("run_wordclock", "Initializing %d strand(s)...", STRANDCNT);
    digitalLeds_initStrands(STRANDS, STRANDCNT);

    time_t now;
    struct tm *timeinfo;
    time(&now);
    timeinfo = localtime(&now);
    if (timeinfo->tm_year < (2016 - 1900)) {
            ESP_LOGI("run_wordclock", "Time is not set yet. Connecting to WiFi and getting time over NTP.");
            refresh_time();
            // update 'now' variable with current time
            time(&now);
    }
    while (true) {
        log_time();
        vTaskDelay(2500 / portTICK_PERIOD_MS);
    }
}

/* Helper function to refresh the system time by connecting to wifi and
 * getting the current time via ntp
 */
static void refresh_time(void)
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    init_wifi();
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);
    init_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
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

    xTaskCreate(&run_wordclock, "run_wordclock", 4096, NULL, 5, NULL);
}
