"""Tests run on your computer against the real ESP32, through its serial logs.

The pytest-embedded plugin supplies `dut` (Device Under Test), flashes the
already-built firmware, and starts reading serial output before each test.
"""

import ipaddress

import pytest
from pytest_embedded_idf.dut import IdfDut


@pytest.mark.hardware
def test_lcd_initialization(dut: IdfDut) -> None:
    """A quick wiring check that does not need working Wi-Fi or internet."""
    # expect() waits for a regular expression in serial output. Parentheses
    # capture the address so we can inspect it with match.group(1).
    match = dut.expect(rb"lcd1602: LCD found at 0x([0-9A-Fa-f]{2})", timeout=15)
    assert int(match.group(1), 16) in (0x27, 0x3F)

    # Discovery alone only proves an I2C device acknowledged its address.
    # This message is emitted after the LCD initialization commands finish.
    dut.expect_exact("main: LCD initialized", timeout=5)


@pytest.mark.hardware
@pytest.mark.network
def test_weather_display_startup(dut: IdfDut) -> None:
    """Check the whole startup path with real Wi-Fi and the live weather API."""
    dut.expect_exact("main: LCD initialized", timeout=15)

    # These checks consume logs in startup order. A timeout identifies the
    # stage that never completed, rather than passing on unrelated output.
    match = dut.expect(rb"wifi_manager: Got IP: (\d+\.\d+\.\d+\.\d+)", timeout=90)
    address = ipaddress.IPv4Address(match.group(1).decode("ascii"))
    assert not address.is_unspecified, "Wi-Fi reported an unspecified IP address"
    dut.expect_exact("wifi_manager: Connected successfully", timeout=5)
    dut.expect_exact("weather_api: HTTP status: 200", timeout=30)

    match = dut.expect(rb"weather_api: Received (\d+) bytes", timeout=5)
    assert int(match.group(1)) > 0, "Weather response was empty"

    # Match the strings actually sent to the two LCD rows. Exact weather
    # values vary, so check sensible ranges instead of today's temperature.
    match = dut.expect(rb"main: LCD row 0: Temp: (-?\d+\.\d) C", timeout=5)
    temperature = float(match.group(1))
    assert -100 <= temperature <= 70, f"Unexpected temperature: {temperature}"
    match = dut.expect(rb"main: LCD row 1: Humidity: (\d+)%", timeout=5)
    humidity = int(match.group(1))
    assert 0 <= humidity <= 100, f"Invalid relative humidity: {humidity}"

    # Reaching this message means parsing and both LCD writes completed.
    # It cannot prove the physical glass shows the right characters.
    dut.expect_exact("main: Weather display ready", timeout=5)
