# Application entry point: combines the four custom components.
# Regenerate configuration when .env changes, including direct idf.py builds.
set(APP_ENV_FILE "${PROJECT_DIR}/.env")
set(APP_CONFIG_SCRIPT "${PROJECT_DIR}/tools/generate_config.py")
set(APP_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  "${APP_ENV_FILE}" "${APP_CONFIG_SCRIPT}")
execute_process(
  COMMAND "${PYTHON}" "${APP_CONFIG_SCRIPT}"
    "${APP_ENV_FILE}" "${APP_CONFIG_DIR}/app_config.h"
  RESULT_VARIABLE APP_CONFIG_RESULT
  ERROR_VARIABLE APP_CONFIG_ERROR
)
if(NOT APP_CONFIG_RESULT EQUAL 0)
  message(FATAL_ERROR "${APP_CONFIG_ERROR}")
endif()

idf_component_register(
  SRCS "main.c"
  INCLUDE_DIRS "." "${APP_CONFIG_DIR}"
  REQUIRES 
    lcd1602
    weather_api
    weather_parser
    wifi_manager
)
