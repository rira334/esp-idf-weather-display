#include "weather_api.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

#define API_URL "https://api.open-meteo.com/v1/forecast"

static const char *TAG = "weather_api";

/*
 * Accumulate HTTP chunks in caller-owned storage, reserving a byte for the terminator.
 */
typedef struct
{
  char *buffer;
  size_t buffer_size;
  size_t length;
} http_response_t;

/*
 * Called by esp_http_client whenever HTTP data arrives.
 */
static esp_err_t http_event_handler(esp_http_client_event_t *event)
{
  http_response_t *response = (http_response_t *)event->user_data;

  switch (event->event_id)
  {
  case HTTP_EVENT_ON_DATA:
    if (response == NULL)
    {
      break;
    }

    // Make sure we don't overflow our buffer.
    size_t available = response->buffer_size - response->length - 1;
    size_t copy_length = event->data_len;

    // Excess response bytes are discarded; truncated JSON may fail during parsing.
    if (copy_length > available)
    {
      copy_length = available;
    }

    if (copy_length > 0)
    {
      // Save data with an offset of response->length
      memcpy(response->buffer + response->length, event->data, copy_length);
      response->length += copy_length;
      response->buffer[response->length] = '\0';
    }
    break;

  default:
    break;
  }

  return ESP_OK;
}

/*
 * Fetch JSON synchronously; parsing and displaying it are separate components.
 */
esp_err_t weather_api_get_current(
    float latitude,
    float longitude,
    char *response_buffer,
    size_t response_size)
{
  if (response_buffer == NULL || response_size == 0)
  {
    return ESP_ERR_INVALID_ARG;
  }

  response_buffer[0] = '\0';

  // Open-Meteo request.
  char url[512];

  snprintf(
      url,
      sizeof(url),

      API_URL
      "?latitude=%.6f"
      "&longitude=%.6f"
      "&current="
      "temperature_2m,"
      "relative_humidity_2m,"
      "apparent_temperature,"
      "weather_code,"
      "wind_speed_10m",

      latitude,
      longitude);

  ESP_LOGI(TAG, "Requesting weather");

  http_response_t response = {
      .buffer = response_buffer,
      .buffer_size = response_size,
      .length = 0,
  };

  esp_http_client_config_t config = {
      .url = url,
      .event_handler = http_event_handler,
      .user_data = &response,
      .crt_bundle_attach = esp_crt_bundle_attach,
      .timeout_ms = 10000,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);

  if (client == NULL)
  {
    ESP_LOGE(TAG, "Could not create HTTP client");
    return ESP_FAIL;
  }

  esp_err_t err = esp_http_client_perform(client);

  if (err != ESP_OK)
  {
    ESP_LOGE(TAG, "HTTP request failed: %s", esp_err_to_name(err));
    esp_http_client_cleanup(client);
    return err;
  }

  int status_code = esp_http_client_get_status_code(client);
  ESP_LOGI(TAG, "HTTP status: %d", status_code);
  esp_http_client_cleanup(client);

  if (status_code != 200)
  {
    ESP_LOGE(TAG, "Weather server returned HTTP %d", status_code);
    return ESP_FAIL;
  }

  if (response.length == 0)
  {
    ESP_LOGE(TAG, "Empty weather response");
    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Received %u bytes", (unsigned int)response.length);

  return ESP_OK;
}
