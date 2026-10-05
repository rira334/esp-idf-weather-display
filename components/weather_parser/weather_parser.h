#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C"
{
#endif

  /* Numeric values copied from Open-Meteo; no pointers into the JSON are retained. */
  typedef struct
  {
    float temperature;
    float apparent_temperature;
    int humidity;
    float wind_speed;
    int weather_code;
  } weather_data_t;

  /* Require a current object with five numeric fields; use the output only on ESP_OK. */
  esp_err_t weather_parser_parse(
      const char *json,
      weather_data_t *weather);

  /* Reserved API: declared here but not implemented or used by the application. */
  const char *weather_parser_code_to_string(
      int weather_code);

#ifdef __cplusplus
}
#endif
