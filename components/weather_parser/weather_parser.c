#include "weather_parser.h"
#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "weather_parser";

/*
 *Convert the current-weather JSON object into values owned by the caller.
 */
esp_err_t weather_parser_parse(const char *json, weather_data_t *weather)
{
  if (json == NULL || weather == NULL)
  {
    return ESP_ERR_INVALID_ARG;
  }

  memset(weather, 0, sizeof(weather_data_t));

  cJSON *root = cJSON_Parse(json);

  if (root == NULL)
  {
    ESP_LOGE(TAG, "Invalid JSON");
    return ESP_FAIL;
  }

  cJSON *current = cJSON_GetObjectItemCaseSensitive(root, "current");

  if (!cJSON_IsObject(current))
  {
    ESP_LOGE(TAG, "'current' object not found");
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  // Require all five requested fields before copying any values into the result.
  cJSON *temperature = cJSON_GetObjectItemCaseSensitive(current, "temperature_2m");
  cJSON *humidity = cJSON_GetObjectItemCaseSensitive(current, "relative_humidity_2m");
  cJSON *apparent_temperature = cJSON_GetObjectItemCaseSensitive(current, "apparent_temperature");
  cJSON *weather_code = cJSON_GetObjectItemCaseSensitive(current, "weather_code");
  cJSON *wind_speed = cJSON_GetObjectItemCaseSensitive(current, "wind_speed_10m");

  if (!cJSON_IsNumber(temperature) ||
      !cJSON_IsNumber(humidity) ||
      !cJSON_IsNumber(apparent_temperature) ||
      !cJSON_IsNumber(weather_code) ||
      !cJSON_IsNumber(wind_speed))
  {
    ESP_LOGE(TAG, "Weather response missing fields");
    cJSON_Delete(root);
    return ESP_FAIL;
  }

  weather->temperature = (float)temperature->valuedouble;
  weather->humidity = humidity->valueint;
  weather->apparent_temperature = (float)apparent_temperature->valuedouble;
  weather->weather_code = weather_code->valueint;
  weather->wind_speed = (float)wind_speed->valuedouble;

  cJSON_Delete(root);
  ESP_LOGI(TAG, "Temperature: %.1f C", weather->temperature);
  return ESP_OK;
}
