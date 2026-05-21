# AGENTS.md

## Project overview
- Parent component is **esp32-wifi-manager**, a modified fork of [tonyp7/esp32-wifi-manager](https://github.com/tonyp7/esp32-wifi-manager), rewritten for [Ruuvi Gateway](https://github.com/ruuvi/ruuvi.gateway_esp.c).
- Built against **ESP-IDF v4.2.5**. Licensed under MIT.

## Scope and entry points
- This directory contains host-side unit tests for the esp32-wifi-manager component.
- Main runtime source lives in `../src/`; tests are in subdirectories here (e.g., `test_http_req/`, `test_http_server_handle_req/`).
- Start reading from `../src/wifi_manager.c`, `../src/wifi_manager_internal.c`, and `../src/http_server.c`.
- Public API surface is in `../src/include/wifi_manager.h` and `../src/include/http_server.h`.

## Architecture you need to understand first
- The system is two cooperating RTOS tasks: Wi-Fi manager task (`wifi_manager_task`) and HTTP server task (`http_server_task`).
- Wi-Fi state machine is message-driven via `wifiman_msg` queue (`../src/wifiman_msg.c`, queue length is 3).
- EventGroup bits in `../src/wifi_manager_internal.h` are the authoritative connection/AP/scan state model.
- ESP-IDF events are translated to internal messages in `../src/wifi_manager_internal.c:wifi_manager_event_handler`.
- HTTP request path is layered: accept/accumulate (`../src/http_server_accept_and_handle_conn.c`) -> parse (`../src/http_req.c`) -> route/auth (`../src/http_server_handle_req.c`) -> serialize/send (`../src/http_server_netconn_resp.c`).

## Request/auth/security flow specifics
- `GET /ap.json` triggers synchronous scan via `wifi_manager_scan_sync()`; scan proceeds channel-by-channel with timer-driven `EVENT_SCAN_NEXT`.
- `GET/POST/DELETE` auth is centralized in `http_server_handle_req_check_auth` and specialized files under `../src/http_server_handle_req_*_auth.c`.
- Optional ECDH transport is negotiated via `Ruuvi-Ecdh-Pub-Key` and `Ruuvi-Ecdh-Encrypted` headers (`../src/http_server_handle_req.c`, `../src/http_server_ecdh.c`).
- Size guards are strict: see `HTTP_SERVER_MAX_REQUEST_SIZE`, `HTTP_SERVER_MAX_ENCRYPTED_CONTENT_SIZE`, `HTTP_SERVER_MAX_UNENCRYPTED_CONTENT_SIZE` usage before adding new payload paths.

## Project conventions (non-generic, follow these)
- Use `os_*` wrappers (`os_mutex`, `os_timer`, `os_malloc`, `os_task`) instead of raw OS/libc APIs in component code.
- Lock with `wifi_manager_lock()/unlock()` when touching shared JSON/config or when queue lifetime can race (`../src/wifiman_msg.c`).
- Keep password logging masked at INFO level (pattern in `../src/http_server_handle_req.c` and `../src/wifi_manager_internal.c`).
- When returning heap-backed HTTP content, use the correct response constructor and ownership enum (`HTTP_CONTENT_LOCATION_HEAP` paths are explicitly freed on error).
- Preserve explicit event-bit logging (`"WIFI_MANAGER:EV_STATE: ..."`) when changing state transitions.

## Build and test workflow
- Unit tests are a CMake project rooted at `CMakeLists.txt` in this directory; they build in both `build/` (Makefiles) and `cmake-build-unit-tests/` (Ninja).
- **Environment setup (required before compiling):** ESP-IDF v4.2.5 strictly requires Python 3.8.
  Set `IDF_PATH`, ensure `python` resolves to `python3.8`, and source ESP-IDF's `export.sh`.
  The recommended way is to use a convenience script — see
  [README.md § Build Environment Setup](../README.md#build-environment-setup) for the full script
  and setup instructions.
```bash
source ~/esp-idf-env.sh
```
- Typical local flow from this directory:
```bash
cmake -S . -B cmake-build-unit-tests -G Ninja
cmake --build cmake-build-unit-tests
ctest --test-dir cmake-build-unit-tests --output-on-failure
```
- Run one suite while iterating:
```bash
ctest --test-dir cmake-build-unit-tests -R test_http_server_netconn_serve_handle_req --output-on-failure
```

## Code coverage
- Coverage flags (`--coverage`, `-ftest-coverage`) are already set per-target in each test subdirectory's `CMakeLists.txt`.
- After running tests, generate a coverage report (requires `lcov` and `genhtml`):
```bash
lcov --capture --directory cmake-build-unit-tests --output-file cmake-build-unit-tests/coverage_all.info
lcov --extract cmake-build-unit-tests/coverage_all.info '*/esp32-wifi-manager/src/*' \
     --output-file cmake-build-unit-tests/coverage_src.info
lcov --list cmake-build-unit-tests/coverage_src.info
genhtml cmake-build-unit-tests/coverage_src.info --output-directory cmake-build-unit-tests/coverage_html
```
- Open `cmake-build-unit-tests/coverage_html/index.html` for a browsable per-file report.

## Test patterns to reuse
- Each test target compiles selected `../src/*.c` directly plus wrapper libs (see `test_http_req/CMakeLists.txt`).
- Coverage and allocator wrapping are standard in tests (`--coverage`, linker wraps for `malloc/calloc/free` in `test_http_server_netconn_serve_handle_req/CMakeLists.txt`).
- Memory-leak assertions commonly use `os_malloc_trace_*` and log expectations (`test_http_server_netconn_serve_handle_req/test_http_server_netconn_serve_handle_req.cpp`).

## Integration boundaries
- External deps used directly in this component: ESP-IDF Wi-Fi/netif/event APIs, lwIP netconn API, mbedTLS (ECDH/AES/SHA256), and cJSON.
- App-level behavior is injected via `wifi_manager_callbacks_t` (`../src/wifi_manager_internal.c`), including custom HTTP GET/POST/DELETE handlers and status/config persistence callbacks.

## Code style
- Code is formatted with **clang-format 14** using the `../.clang-format` config (based on BARR-C:2018).
- To reformat all source and test files from the parent directory:
```bash
cd .. && scripts/clang_format_all.sh
```
  This runs `clang-format-14 --recursive --in-place` over `src/` and `tests/test*/`.
- After formatting, verify no diff remains: `git diff --exit-code`.

## CI / GitHub Actions (`../.github/workflows/`)
Three workflows run on push/PR to `master`:

1. **`code-style.yml` (Clang-Format)** — installs clang-format-14, runs `scripts/clang_format_all.sh`, and fails if any file changes.
2. **`google-tests.yml` (Google Tests)** — on ubuntu-22.04, sets up Python 3.8, installs ESP-IDF v4.2.5 (cached), builds tests with Ninja, and runs `ctest --output-on-failure`. Requires `de_DE.UTF-8` locale.
3. **`sonar-scan.yml` (SonarCloud Analysis)** — builds tests with `--coverage`, generates `gcovr` coverage in SonarQube XML format, and uploads to SonarCloud (project key: `ruuvi_esp32-wifi-manager`, org: `ruuvi`).

Key CI details for reproducing locally:
- CI uses `ubuntu-22.04`, `gcc/g++`, `cmake`, `ninja-build`.
- ESP-IDF is cloned to `~/esp/esp-idf` in CI (vs `~/esp-idf-v4.2.5` locally).
- SonarCloud coverage uses `gcovr -r . --sonarqube` (not `lcov`) from the repository root.
- The `de_DE.UTF-8` locale is required by certain tests — install locally with `sudo locale-gen de_DE.UTF-8` if missing.

