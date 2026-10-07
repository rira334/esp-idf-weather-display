idf_component_register(
    SRCS "weather_api.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_http_client
        mbedtls
)
