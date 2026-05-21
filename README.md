
This is modification of *esp32-wifi-manager*, which you can find here: https://github.com/tonyp7/esp32-wifi-manager

The original component has been rewritten quite extensively in order to best integrate it with "Ruuvi Gateway" https://github.com/ruuvi/ruuvi.gateway_esp.c.git

---

# What is esp32-wifi-manager?

*esp32-wifi-manager* is an esp-idf component for ESP32 and ESP8266 that enables easy management of wifi networks through a web application.

*esp32-wifi-manager* is **lightweight** (8KB of task stack in total) and barely uses any CPU power through a completely event driven architecture. It's an all in one wifi scanner, http server & dns daemon living in the least amount of RAM possible.

For real time constrained applications, *esp32-wifi-manager* can live entirely on PRO CPU, leaving the entire APP CPU untouched for your own needs.

*esp32-wifi-manager* will automatically attempt to re-connect to a previously saved network on boot, and it will start its own wifi access point through which you can manage wifi networks if a saved network cannot be found and/or if the connection is lost.

*esp32-wifi-manager* is an esp-idf project that compiles successfully with the esp-idf v4.2.5 
release. You can simply copy the project and start adding your own code to it.

# License
*esp32-wifi-manager* is MIT licensed. As such, it can be included in any project, commercial or not, as long as you retain original copyright. Please make sure to read the license file.

## Build Environment Setup

This component is built as part of the [Ruuvi Gateway firmware](https://github.com/ruuvi/ruuvi.gateway_esp.c) using **ESP-IDF v4.2.5**.

### Python 3.8 requirement

ESP-IDF v4.2.5 **requires Python 3.8**. The build will fail with newer Python versions (3.9+).
If your system default Python is newer, you need to ensure that `python` resolves to `python3.8`
before sourcing ESP-IDF's `export.sh`. The environment setup script below handles this automatically.

### Environment setup script (`esp-idf-env.sh`)

Before compiling or running unit tests, you must set up the ESP-IDF environment.
The minimal approach is:

```shell
export IDF_PATH="$HOME/esp-idf-v4.2.5"
source "$IDF_PATH/export.sh"
```

However, this only works if your system default `python` is already Python 3.8.
For systems with a newer default Python, create a helper script (e.g. `~/esp-idf-env.sh`)
with the following content and `source` it before building:

```bash
#!/usr/bin/env bash

# ESP-IDF 4.2.5 root — adjust this path to your installation
export IDF_PATH="$HOME/esp-idf-v4.2.5"

# Keep the venv path fixed for this IDF version
export IDF_PYTHON_ENV_PATH="$HOME/.espressif/python_env/idf4.2_py3.8_env"

# Locate Python 3.8 and fail fast if not found
PYTHON38="$(command -v python3.8 2>/dev/null)" || true
if [ -z "$PYTHON38" ]; then
    echo "ERROR: python3.8 not found in PATH. Install it first (see README)." >&2
    return 1 2>/dev/null || exit 1
fi

# Make Python 3.8 appear first — create a shim so that "python" resolves to python3.8
PY38_SHIM_DIR="$HOME/.local/esp-idf-py38-shim"
mkdir -p "$PY38_SHIM_DIR"
cat > "$PY38_SHIM_DIR/python" <<PYEOF
#!/usr/bin/env bash
exec "$PYTHON38" "\$@"
PYEOF
chmod +x "$PY38_SHIM_DIR/python"
export PATH="$PY38_SHIM_DIR:$PATH"

# Load ESP-IDF environment (adds toolchain to PATH, sets up idf.py, etc.)
. "$IDF_PATH/export.sh"
```

**Note:** This script is not part of the repository because it depends on host-specific paths.
Adjust `IDF_PATH` to match your ESP-IDF installation location.

Usage:
```shell
source ~/esp-idf-env.sh
```

### Prerequisites (Ubuntu 22.04)

```shell
sudo apt-get update
sudo apt-get install -y gcc g++ cmake make ninja-build mtools
sudo apt-get install -y git wget libncurses-dev flex bison gperf ccache libffi-dev libssl-dev
sudo apt-get install -y locales software-properties-common
sudo add-apt-repository -y ppa:deadsnakes/ppa
sudo apt-get update
sudo apt-get install -y python3.8 python3.8-venv python3.8-dev
sudo locale-gen de_DE.UTF-8   # Required by some unit tests
```

## Building and Running Unit Tests

Host-side unit tests use **Google Test (C++)** and a **POSIX FreeRTOS simulator**. They live in
`tests/` and form an independent CMake project.

```shell
# Set up the environment first (see above)
source ~/esp-idf-env.sh

# Build and run all tests
cmake -S tests -B tests/cmake-build-unit-tests -G Ninja
cmake --build tests/cmake-build-unit-tests
ctest --test-dir tests/cmake-build-unit-tests --output-on-failure

# Run a single test suite
ctest --test-dir tests/cmake-build-unit-tests -R test_http_server_netconn_serve_handle_req --output-on-failure
```

### Code coverage

Coverage flags (`--coverage`, `-ftest-coverage`) are already set per-target in each test
subdirectory's `CMakeLists.txt`. After running tests, generate a local coverage report:

```shell
lcov --capture --directory tests/cmake-build-unit-tests --output-file tests/cmake-build-unit-tests/coverage_all.info
lcov --extract tests/cmake-build-unit-tests/coverage_all.info '*/esp32-wifi-manager/src/*' \
     --output-file tests/cmake-build-unit-tests/coverage_src.info
lcov --list tests/cmake-build-unit-tests/coverage_src.info
genhtml tests/cmake-build-unit-tests/coverage_src.info --output-directory tests/cmake-build-unit-tests/coverage_html
```

Open `tests/cmake-build-unit-tests/coverage_html/index.html` for a browsable per-file report.

