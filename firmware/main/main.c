#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_err.h"
#include "driver/i2c_master.h"

#include "config.h"
#include "mqtt.h"
#include "wifi.h"

#include "bmp280.h"
#include "pms5003.h"

#define TAG "main"

#define GPIO_I2C_SCL 3
#define GPIO_I2C_SDA 5
#define GPIO_PMS5003_TX 39
#define GPIO_PMS5003_RX 37
#define GPIO_PMS5003_MODE 35

TaskHandle_t task_mqtt;
TaskHandle_t task_bmp280;
TaskHandle_t task_pms5003;

bmp280_device_t bmp280;
pms5003_device_t pms5003;

int nvs_setup(void);
int netif_setup(void);
void bmp280_task(void* pvParameters);
void pms5003_task(void* pvParameters);

void app_main(void) {
    i2c_master_bus_handle_t i2c_bus;

    i2c_master_bus_config_t i2c_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = GPIO_I2C_SCL,
        .sda_io_num = GPIO_I2C_SDA,
        .flags.enable_internal_pullup = true,
    };

    bmp280_config_t bmp280_config = {
        .standby_time = BMP280_STANDBY_4000_MS,
        .temperature_oversampling = BMP280_OVERSAMPLING_X1,
        .pressure_oversampling = BMP280_OVERSAMPLING_X4,
        .filter = BMP280_FILTER_OFF,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_config, &i2c_bus));
    ESP_ERROR_CHECK(bmp280_init(&bmp280, &bmp280_config, i2c_bus));
    ESP_ERROR_CHECK(bmp280_set_power_mode(&bmp280, BMP280_POWER_MODE_NORMAL));

    ESP_ERROR_CHECK(pms5003_init(&pms5003, UART_NUM_0, GPIO_PMS5003_TX, GPIO_PMS5003_RX, GPIO_PMS5003_MODE));
    ESP_ERROR_CHECK(pms5003_set_mode(&pms5003, PMS5003_MODE_SLEEP));

    ESP_ERROR_CHECK(nvs_setup());
    ESP_ERROR_CHECK(netif_setup());
    ESP_ERROR_CHECK(wifi_setup());
    ESP_ERROR_CHECK(mqtt_setup());

    xTaskCreate(mqtt_task, "mqtt", 2048, NULL, 10, &task_mqtt);
    xTaskCreate(bmp280_task, "bmp280", 2048, NULL, 10, &task_bmp280);
    xTaskCreate(pms5003_task, "pms5003", 2048, NULL, 10, &task_pms5003);
}

void bmp280_task(void* pvParameters) {
    int32_t temperature;
    uint32_t pressure;

    while (1) {
        if (bmp280_get_temperature_degC_x100_int(&bmp280, &temperature) == BMP280_OK) {
            mqtt_publish_float("weather/temperature", temperature / 100.0);
        }

        if (bmp280_get_pressure_Pa_x1_int(&bmp280, &pressure) == BMP280_OK) {
            mqtt_publish_float("weather/pressure", pressure / 100.0);
        }

        vTaskDelay((1 * 60 * 1000) / portTICK_PERIOD_MS);
    }
}

void pms5003_task(void* pvParameters) {
    uint16_t pm1, pm2, pm10;

    while (1) {
        pms5003_set_mode(&pms5003, PMS5003_MODE_NORMAL);

        vTaskDelay((30 * 1000) / portTICK_PERIOD_MS);

        if (pms5003_get_pm(&pms5003, &pm1, &pm2, &pm10) == PMS5003_OK) {
            mqtt_publish_int("weather/pm1", pm1);
            mqtt_publish_int("weather/pm2.5", pm2);
            mqtt_publish_int("weather/pm10", pm10);
        }

        pms5003_set_mode(&pms5003, PMS5003_MODE_SLEEP);

        vTaskDelay((10 * 60 * 1000) / portTICK_PERIOD_MS);
    }
}

int nvs_setup(void) {
    esp_err_t err;

    err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
       if (nvs_flash_erase() == ESP_OK) {
            err = nvs_flash_init();
       }
    }

    if (err != ESP_OK)
        return 1;

    return 0;
}

int netif_setup(void) {
    if (esp_netif_init() != ESP_OK)
        return 1;

    if (esp_event_loop_create_default() != ESP_OK)
        return 1;

    esp_netif_t* netif = esp_netif_create_default_wifi_sta();

    if (esp_netif_set_hostname(netif, CONFIG_HOSTNAME) != ESP_OK)
        return 1;

    return 0;
}
