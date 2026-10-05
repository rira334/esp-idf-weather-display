# Weather parsing: cJSON validates and extracts the response fields.
idf_component_register(
    SRCS "weather_parser.c"
    INCLUDE_DIRS "."
    REQUIRES cjson
)
