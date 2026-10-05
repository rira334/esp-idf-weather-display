#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Initialize once and wait for an IP address, with up to ten disconnection retries.
   * Returns connection status; fatal ESP-IDF setup errors use ESP_ERROR_CHECK. */
  esp_err_t wifi_manager_init(
      const char *ssid,
      const char *password);

  /* Read the connection state last recorded by the Wi-Fi event handler. */
  bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif