# ESP32 I2C weather display

This ESP-IDF project connects an ESP32 to Wi-Fi, fetches current weather for the configured coordinates, and shows temperature and relative humidity on a 16×2 LCD with a PCF8574 I2C backpack.

Weather comes from an HTTPS request to Open-Meteo, rather than a local sensor. The application fetches it **once at startup** and leaves those values on screen until the board restarts. It does not currently refresh the weather periodically.

## Startup flow

1. Initialize the LCD and show `Connecting WiFi`.
2. Connect to Wi-Fi and wait for an IP address. Show `WiFi connected` on success.
3. Fetch the current-weather JSON into a 2048-byte buffer.
4. Parse the response into a `weather_data_t` structure.
5. Display temperature on row 1 and humidity on row 2.

Example display (illustrative values):

```text
Temp: 18.5 C
Humidity: 65%
```

Connection, request, or parsing failures show `WiFi error`, `Weather error`, or `Parse error`, respectively, and stop startup. Fatal setup errors and LCD write failures use `ESP_ERROR_CHECK`, which aborts the application rather than showing an error message.

## Components

| Directory | Responsibility | Main API |
| --- | --- | --- |
| `main/` | Coordinates startup, formats two LCD lines, and keeps the application task alive. | `app_main()` |
| `components/lcd1602/` | Controls the LCD through the PCF8574 backpack using the ESP-IDF I2C master driver. | `lcd1602_init()`, `lcd1602_clear()`, `lcd1602_set_cursor()`, `lcd1602_print()`, `lcd1602_write_char()` |
| `components/wifi_manager/` | Initializes NVS, networking, and station Wi-Fi; handles connection events and retries. | `wifi_manager_init()`, `wifi_manager_is_connected()` |
| `components/weather_api/` | Makes the HTTPS request and collects response chunks into a caller-provided buffer. | `weather_api_get_current()` |
| `components/weather_parser/` | Validates the JSON with cJSON and copies numeric weather values into a structure. | `weather_parser_parse()` |

### LCD driver

The driver uses I2C controller 0 at 100 kHz. It probes address `0x27` first, then `0x3F`. The assumed backpack pin mapping is RS on bit 0, RW on bit 1, Enable on bit 2, backlight on bit 3, and LCD D4–D7 on bits 4–7.

Each command or character is sent as two nibbles in 4-bit mode. Data settles before Enable is raised, and the driver waits after each transfer. Clear and home commands wait 3 ms using a microsecond delay, so the wait cannot round down to zero at the configured FreeRTOS tick rate.

#### How the LCD interface works

The LCD1602 is a 16-column by 2-row character display. Its controller (commonly HD44780-compatible) receives commands and character bytes from the microcontroller:

- **RS (Register Select)** selects what the byte means: `0` for a command (such as clearing the display or positioning the cursor), `1` for character data (such as the byte representing `A`).
- **RW (Read/Write)** selects transfer direction: `0` writes to the LCD, `1` reads from it. This driver only writes; many backpacks tie RW low.
- **EN (Enable)** is pulsed to tell the LCD to latch the data currently on its data pins.
- **Backlight** powers the light behind the characters. It is independent of the text/control signals and is switched through the backpack; it does not itself send data to the display.

The controller works with 8-bit bytes, but this driver uses 4-bit mode to save four connections. A byte is split into two 4-bit **nibbles** and sent high nibble first, then low nibble, with an Enable pulse for each. For example, `A` is `0x41` (`0100 0001`), so the LCD receives `0100` followed by `0001` and combines them into the original byte. Both commands and character data are sent this way; RS tells the controller how to interpret the completed byte.

Commands are bytes whose bit patterns request controller operations. Common examples are `0x01` (clear display), `0x02` (return cursor home), `0x0C` (display on, cursor hidden), `0x06` (advance cursor after writing), and `0x80` (set cursor to the start of the first row). The controller needs time to execute some commands, especially clear and home, so the driver waits after transfers.

The driver's `lcd_hw_init()` function in `components/lcd1602/lcd1602.c` performs the power-on setup. It first turns on the backlight and waits 100 ms for the LCD to power up. It then sends the `0x3` nibble three times, with delays, to bring the controller into a known state even if it initially expects 8-bit transfers. It sends `0x2` next to select 4-bit mode. These are individual nibbles, not complete bytes. Once the controller is in 4-bit mode, the function sends full command bytes (each encoded as two nibbles): `0x28` selects 4-bit, two-line operation; `0x08` keeps the display off during setup; `0x01` clears it; `0x06` sets the cursor to advance after each character; and `0x0C` turns the display on with the cursor hidden. The waits give the controller time to power up and execute its commands, particularly the slower clear command.

Cursor positions are zero-based: columns 0–15 and rows 0–1. Printing advances the cursor without clipping text or handling row wrapping. Initialize the driver once and call it from one task; it has no locking for concurrent writers.

### Wi-Fi manager

`wifi_manager_init(ssid, password)` configures station mode and blocks until the board gets an IP address or the retry limit is reached. Disconnection events trigger up to ten retries; acquiring an IP resets the retry count. Registered handlers continue handling disconnections after initialization returns. The configuration requires at least WPA2-PSK authentication.

`wifi_manager_is_connected()` reports the latest state recorded by the event handler. The application currently uses the initialization result rather than polling this function.

### Weather API

`weather_api_get_current(latitude, longitude, response, response_size)` requests `temperature_2m`, `relative_humidity_2m`, `apparent_temperature`, `weather_code`, and `wind_speed_10m` from the Open-Meteo forecast endpoint. It uses the ESP-IDF certificate bundle for HTTPS verification and a 10-second HTTP timeout.

The caller owns the response buffer. The event handler appends incoming chunks and keeps the buffer null-terminated. A successful call means the request completed with HTTP 200 and a nonempty body; it does not validate JSON. If the buffer is too small, excess bytes are discarded, which may cause parsing to fail.

### Weather parser

`weather_parser_parse(json, &weather)` requires a `current` object containing all five requested fields as numbers. It copies temperature, apparent temperature, humidity, weather code, and wind speed into `weather_data_t`, then frees the JSON tree. Only temperature and humidity are displayed by `main`.

Null arguments, invalid JSON, and missing or nonnumeric fields return an error. Use the output only when the function returns `ESP_OK`. `weather_parser_code_to_string()` is declared in the header but has no implementation and is not called by the application.

## Hardware and configuration

The current application uses these connections:

| Signal | ESP32 pin |
| --- | --- |
| LCD backpack SDA | GPIO 21 |
| LCD backpack SCL | GPIO 22 |
| LCD backpack GND | GND |

Power the LCD/backpack according to its hardware requirements and use I2C signal levels compatible with the ESP32.

Settings are loaded from the root `.env` file at build time. Your existing settings have been moved there. For a fresh checkout, run `cp .env.example .env` and edit it before building:

- `WIFI_SSID` and `WIFI_PASSWORD`: your network credentials.
- `SDA_GPIO` and `SCL_GPIO`: your I2C pins.
- `LATITUDE` and `LONGITUDE`: the location to request. Currently `53.55`, `9.99` (Hamburg).

The `.env` file is ignored by Git. Values are literal text, with optional surrounding single or double quotes. Shell substitutions and variable expansion are not performed; use full-line comments rather than inline comments. Credentials are compiled into the firmware, so `.env` changes require rebuilding and flashing.

CMake runs `tools/generate_config.py` to generate `app_config.h` inside the build directory. It watches `.env` for changes, so both Makefile and direct `idf.py` builds use the same settings.

## Build and run

With the ESP-IDF environment activated, run from the project directory:

```sh
make build
make flash PORT=/dev/cu.usbserial-XXXX
make monitor PORT=/dev/cu.usbserial-XXXX
# Or flash and open the monitor in one command:
make flash-monitor PORT=/dev/cu.usbserial-XXXX
```

`make` defaults to `build`. Omit `PORT` to let ESP-IDF select a port. `flash` builds as needed. You can override the command with `IDF_PY=/path/to/idf.py`. Direct commands also work:

```sh
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with your board's serial port. Exit the monitor with `Ctrl-]`.

Each component's `CMakeLists.txt` registers its source, header directory, and ESP-IDF dependencies. The component manifest includes cJSON and the inherited LED-strip dependency; the application does not use the LED-strip library. `managed_components/` contains dependency code managed by ESP-IDF.

## Remaining blink-example files

The CMake project is still named `blink`, so the firmware artifacts are named `blink.elf` and `blink.bin`. `main/Kconfig.projbuild` contains inherited blink settings that the current application does not read. `pytest_weather.py` provides hardware integration tests for LCD initialization and full weather-display startup. See [the hardware testing tutorial](docs/hardware-tests.md) for setup, commands, an explanation of fixtures and assertions, and manual LCD checks.

## Hardware tests

See [Learning hardware tests](docs/hardware-tests.md). The tests flash the built firmware and check serial output on a real ESP32; the full startup test also requires working Wi-Fi and internet. LCD character appearance is checked manually. Test dependencies are listed in `requirements-test.txt`.
