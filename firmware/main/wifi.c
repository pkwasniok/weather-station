#include "wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"

#include "config.h"

void wifi_callback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

int wifi_setup(void) {
    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init_config) != ESP_OK)
        return 1;

    esp_event_handler_instance_t handler_any_id;
    if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_callback, NULL, &handler_any_id) != ESP_OK)
        return 1;

    wifi_config_t config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK,
        }
    };

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK)
        return 1;

    if (esp_wifi_set_config(WIFI_IF_STA, &config) != ESP_OK)
        return 1;

    if (esp_wifi_start() != ESP_OK)
        return 1;

    return 0;
}

void wifi_callback(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    switch ((wifi_event_t) event_id) {
        case WIFI_EVENT_STA_START:
        case WIFI_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            break;

        default:
            break;
    }
}

