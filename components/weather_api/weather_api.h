#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Requires connected Wi-Fi. Fetches JSON into caller storage (size includes terminator).
   * Returns ESP_OK for a nonempty HTTP 200 response; overflow is truncated. */
  esp_err_t weather_api_get_current(
      float latitude,
      float longitude,
      char *response,
      size_t response_size);

#ifdef __cplusplus
}
#endif
