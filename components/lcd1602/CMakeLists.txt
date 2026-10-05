# LCD driver: I2C master support is provided by ESP-IDF.
idf_component_register(
    SRCS "lcd1602.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c
)
