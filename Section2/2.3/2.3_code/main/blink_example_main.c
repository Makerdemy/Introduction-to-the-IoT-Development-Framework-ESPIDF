
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sdkconfig.h"

static const char *TAG = "Blink_example";

#define BLINK_GPIO CONFIG_BLINK_GPIO


void app_main(void)
{
    gpio_reset_pin(BLINK_GPIO);
    gpio_set_direction (BLINK_GPIO,GPIO_MODE_OUTPUT);
    while (1) {
        gpio_set_level(BLINK_GPIO, 0);
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Turning the LED %s!","OFF");
        gpio_set_level(BLINK_GPIO, 1);
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Turning the LED %s!","ON");
    }
}
