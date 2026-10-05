# Learning hardware tests with this project

A hardware test runs Python on your computer while the C firmware runs on the real ESP32. They communicate through the USB serial port. Our Python tests watch the same messages you see in `idf.py monitor` and assert that important startup steps succeed.

```text
Computer: pytest → flash/reset ESP32 → read serial messages → check assertions
Board:    LCD initialization → Wi-Fi → HTTPS → JSON parsing → LCD writes
```

These are integration tests: they exercise several components together, including physical I2C hardware and, for the full startup test, a real network. They are different from unit tests, which isolate a function using controlled inputs.

## Prepare the board and computer

1. Connect your ESP32 by USB and connect the LCD using the pins in `.env`.
2. Configure your Wi-Fi credentials and coordinates in `.env`.
3. Activate the ESP-IDF environment as you normally do for a build.
4. Close `idf.py monitor`, IDE serial monitors, and other programs using the board's port.
5. Install the Python test tools in a separate environment and build the firmware:

```sh
python -m venv .venv-hardware
.venv-hardware/bin/python -m pip install -r requirements-test.txt
idf.py build
```

Keep using your activated ESP-IDF shell for `idf.py`; use the test environment's Python explicitly for pytest. The Python virtual environment keeps test packages separate from ESP-IDF's tooling. On Windows, use `.venv-hardware\Scripts\python.exe` instead of `.venv-hardware/bin/python`.

## Run the tests

Replace the port below with your board's port. The target must match the built firmware; this project's current board is ESP32.

```sh
ESPBAUD=115200 .venv-hardware/bin/python -m pytest pytest_weather.py -v -s \
  --embedded-services esp,idf --target esp32 \
  --app-path . --build-dir build --port /dev/cu.usbserial-110
```

This command flashes the firmware already in `build/`, resets the board, and captures its output. It does not build the C code for you. Rebuild first after changing the firmware or `.env`. Each test gets a fresh device fixture; the plugin may cache flashing when the binary is unchanged.

- `ESPBAUD=115200` selects a conservative upload speed for the ESP-IDF flashing service. This setting is separate from serial-monitor speed.
- `-v` shows each test's name and result.
- `-s` shows serial output as the tests run.
- `--embedded-services esp,idf` enables Espressif serial/flashing support and ESP-IDF build metadata.
- `--port` chooses the physical board. `--target` describes its chip.

To check only LCD startup without requiring Wi-Fi:

```sh
ESPBAUD=115200 .venv-hardware/bin/python -m pytest pytest_weather.py -v -s -m 'hardware and not network' \
  --embedded-services esp,idf --target esp32 \
  --app-path . --build-dir build --port /dev/cu.usbserial-110
```

To list tests without connecting to or flashing a board:

```sh
.venv-hardware/bin/python -m pytest --collect-only -q
```

## Read one test step by step

Pytest discovers functions starting with `test_` in `pytest_weather.py`. Our `pytest.ini` explicitly selects this filename because it does not follow pytest's usual `test_*.py` naming pattern.

The `dut` argument is a **fixture**: pytest asks the installed plugin to prepare it before calling the function. It represents the Device Under Test and gives us access to incoming serial output. You do not create it manually.

```python
match = dut.expect(rb"weather_api: Received (\d+) bytes", timeout=5)
assert int(match.group(1)) > 0, "Weather response was empty"
```

`dut.expect()` waits up to five seconds for a matching message. `rb` creates a raw bytes pattern for serial data. `\d+` matches one or more digits; parentheses capture those digits. `group(1)` retrieves the captured bytes and `int()` converts them to a number. `assert` fails the test if the condition is false.

`dut.expect_exact()` waits for literal text rather than a regular expression. Each expectation consumes output through its match, so our checks follow startup order. Reordering them can make a test wait for a message that was already consumed.

If a message never arrives, the expectation fails with a timeout and captured serial output. This is useful evidence: a Wi-Fi timeout suggests checking credentials/network, while an HTTP timeout suggests checking internet access or the weather service. A failure identifies what to investigate; it does not automatically prove the firmware has a bug.

## What each test proves

| Test | Checks | Dependencies |
| --- | --- | --- |
| `test_lcd_initialization` | Supported backpack address acknowledges; initialization commands finish without an I2C error. | Board, USB, LCD/backpack |
| `test_weather_display_startup` | LCD initialization, assigned IPv4 address, connection success, HTTP 200, nonempty response, successful parsing, sensible temperature/humidity, completed LCD writes. | Same hardware, Wi-Fi, internet, live weather service |

The C application logs each LCD row after sending it. This lets the test check the text actually handed to the driver rather than just trusting an earlier parser log. Humidity must be 0–100%; temperature uses a deliberately broad sanity range. These checks do not compare the weather against a reference sensor and do not test every weather field.

A serial message cannot tell us what appears on the LCD glass. The backpack can acknowledge I2C even with a disconnected or incorrectly wired LCD controller. Passing tests therefore still need the visual checks below.

## Manual LCD check

After the automated tests, run `idf.py -p /dev/cu.usbserial-110 monitor` and reset the board to observe startup again:

1. Confirm `Connecting WiFi` starts with `C` and no scrambled characters.
2. Confirm `WiFi connected` appears when the connection succeeds. It may be brief.
3. Compare the temperature and humidity on screen with the `LCD row 0` and `LCD row 1` serial messages.
4. Wait at least 30 seconds. Both rows should remain stable without flickering or random characters.
5. Reset the board several times to check whether the earlier intermittent corruption returns.

No automatic test here measures character appearance, contrast, backlight quality, or long-term stability. Proving those automatically would require extra instrumentation such as a camera or a controller readback mechanism.

## Try a small learning exercise

Change the expected final text in Python from `Weather display ready` to `Weather display READY`, then run only `test_weather_display_startup` using `-k weather_display_startup`. The firmware has not changed, so the test should time out at the last step. Read the captured output to see the real message, then restore the expected text.

This demonstrates the difference between a working device and a correct test expectation. When adding a new feature, first decide what observable result would prove it worked, then add an assertion for that result.

## Troubleshooting and further reading

- `fixture 'dut' not found`: install `requirements-test.txt` into the Python environment used to run pytest.
- Port busy or permission error: close serial monitors, confirm the port, and check OS serial permissions.
- LCD timeout: check power, wiring, address, and the firmware's I2C error output.
- Wi-Fi timeout: verify `.env`, rebuild, and confirm the network is available.
- HTTP or parsing failure: read the firmware logs; this test depends on a live external service.
- Old or missing log messages: rebuild before running pytest so it flashes the updated firmware.

See Espressif's [pytest-embedded services](https://docs.espressif.com/projects/pytest-embedded/en/latest/concepts/services.html) and [fixture and lifecycle explanation](https://docs.espressif.com/projects/pytest-embedded/en/latest/concepts/key-concepts.html) for how the plugin prepares, flashes, and reads a board.
