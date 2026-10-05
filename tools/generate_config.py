import math
from pathlib import Path
import sys

def generate(source, destination):
    values = {}
    for number, line in enumerate(source.read_text().splitlines(), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        key, separator, value = line.partition("=")
        key, value = key.strip(), value.strip()
        if not separator or key in values:
            raise ValueError(f"Invalid or duplicate setting on line {number}")
        if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
            value = value[1:-1]
        values[key] = value

    definitions = []
    for key in ("WIFI_SSID", "WIFI_PASSWORD"):
        value = values[key]
        if "\0" in value:
            raise ValueError(f"{key} must not contain null bytes")
        # Encode UTF-8 bytes as octal escapes, preserving arbitrary literal text.
        literal = '"' + ''.join(f"\\{byte:03o}" for byte in value.encode("utf-8")) + '"'
        definitions.append(f"#define {key} {literal}")
    for key in ("SDA_GPIO", "SCL_GPIO"):
        value = int(values[key])
        if value < 0:
            raise ValueError(f"{key} must be nonnegative")
        definitions.append(f"#define {key} {value}")
    for key, limit in (("LATITUDE", 90), ("LONGITUDE", 180)):
        value = float(values[key])
        if not math.isfinite(value) or not -limit <= value <= limit:
            raise ValueError(f"{key} is outside its valid range")
        definitions.append(f"#define {key} {value!r}f")
    content = "/* Generated from .env; do not edit. */\n#pragma once\n\n" + "\n".join(definitions) + "\n"
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists() or destination.read_text() != content:
        destination.write_text(content)

if __name__ == "__main__":
    try:
        generate(Path(sys.argv[1]), Path(sys.argv[2]))
    except FileNotFoundError:
        sys.exit("Missing .env: copy .env.example to .env and configure it.")
    except (KeyError, ValueError) as error:
        sys.exit(f"Invalid .env configuration: {error}")
