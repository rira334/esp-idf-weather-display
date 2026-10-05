#include <stdio.h>

#include "app_config.h"

#include "esp_err.h"
#include "esp_log.h"

#include "wifi_manager.h"
#include "weather_api.h"
#include "weather_parser.h"
#include "lcd1602.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

/* Coordinate startup: LCD, Wi-Fi, one weather request, parsing, then display. */
void app_main(void)
{
  ESP_ERROR_CHECK(lcd1602_init(SDA_GPIO, SCL_GPIO));
  ESP_LOGI(TAG, "LCD initialized");
  lcd1602_clear();
  lcd1602_print("Connecting WiFi");

  esp_err_t err =
      wifi_manager_init(
          WIFI_SSID,
          WIFI_PASSWORD);

  if (err != ESP_OK)
  {
    lcd1602_clear();
    lcd1602_print("WiFi error");

    ESP_LOGE(
        TAG,
        "Wi-Fi connection failed");

    return;
  }

  lcd1602_clear();
  lcd1602_print("WiFi connected");

  /* The fetched weather is unchanged, so leave the display in place. */
  while (1)
  {
    /* The HTTP component fills this buffer; the parser copies out numeric values. */
    char json[2048];
    weather_data_t weather;
    err = weather_api_get_current(
        LATITUDE,
        LONGITUDE,
        json,
        sizeof(json));
    if (err != ESP_OK)
    {
      lcd1602_clear();
      lcd1602_print("Weather error");
      return;
    }

    err = weather_parser_parse(
        json,
        &weather);
    if (err != ESP_OK)
    {
      lcd1602_clear();
      lcd1602_print("Parse error");
      return;
    }

    /* Each LCD row holds 16 characters, plus a null terminator for snprintf. */
    char line[17];
    lcd1602_clear();
    snprintf(
        line,
        sizeof(line),
        "Temp: %.1f C",
        weather.temperature);
    lcd1602_set_cursor(0, 0);
    lcd1602_print(line);
    ESP_LOGI(TAG, "LCD row 0: %s", line);

    snprintf(
        line,
        sizeof(line),
        "Humidity: %d%%",
        weather.humidity);
    lcd1602_set_cursor(0, 1);
    lcd1602_print(line);
    ESP_LOGI(TAG, "LCD row 1: %s", line);
    ESP_LOGI(TAG, "Weather display ready");

    vTaskDelay(pdMS_TO_TICKS(60 * 1000));
  }
}
