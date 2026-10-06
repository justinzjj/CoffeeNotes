[简体中文](2026-10-06-coffeenotes.zh_CN.md) · **English**

# CoffeeNotes Implementation Plan

**Goal:** Record coffee types in a calendar and provide editable pour-over references with a live timer on AI Passport.

**Architecture:** Pure C dates, records, recipes and input state are independent of ESP-IDF/LVGL. One application worker owns state and durable writes, and updates a custom LVGL UI under the BSP lock. A separate queued network service owns Wi-Fi, a temporary WPA2 SoftAP/HTTP setup page and SNTP. No audio or Bluetooth stack is needed.

**Tech stack:** ESP-IDF 5.5.3, ESP32-C3, BSP, LVGL 9.5, NVS, esp_http_server, SNTP, Noto CJK application subset.

The user authorized autonomous implementation after answering key requirements. Preserve the CarCard checkout, use feature/coffeenotes in this worktree, and do not commit/push or flash.

## 1. Dates, records, recipes and interactions

- [x] Add main/coffee_model.h and .c and tests/test_coffee_model.c. Cover leap years (2000 vs 2100), month/year boundaries, Monday-first grid, coffee counts, capacity, explicit deletion and monotonic pause/resume independent of wall-clock jumps.
- [x] Add an explicit versioned little-endian record codec with CRC and validate dates, type, recipe and parameter ranges before accepting persisted data.
- [x] Keep a maximum of 512 records without silently dropping old entries. A save action must wait for a durable result before the UI claims success. Show storage errors and retain the draft for retry.
- [x] On cold boot, a stored date is only a draft until SNTP or manual confirmation. Bound supported dates to 2020-2099. The user can correct record dates and review per-day records.
- [x] Provide three clearly labeled starter recipes, editable powder/water/temperature/target duration, proportional cumulative-water stages and a timer that can pause, cancel or finish into a hand-brew record.

Run: `cc -std=c11 -Wall -Wextra -Werror -Imain tests/test_coffee_model.c main/coffee_model.c -o /tmp/test_coffee_model && /tmp/test_coffee_model`.
Expected: all date, record, codec and timer assertions pass. Run the tests first to observe missing implementation.

## 2. Network service

- [x] Add main/coffee_network.h and .c with thread-safe snapshots and queued setup/cancel/forget commands. All network and HTTP work stays outside LVGL and button callbacks.
- [x] Use a five-minute temporary WPA2 AP with random eight-digit passphrase shown on the device, one phone client and http://192.168.4.1. Bound and validate the form; reject oversized, malformed or duplicate fields and use a session token.
- [x] Test the candidate STA credentials before storing them. Retain old credentials on connection failure, stop HTTP/AP after success/cancel/timeout, and use bounded retry/backoff for the saved STA.
- [x] Set local time to CST-8, synchronize SNTP only on an IP-connected STA, and expose validated clock status separately from network association. Do not print passwords or embed router credentials.
- [x] Add pure form parsing tests including encoded UTF-8 SSIDs, maximum lengths, invalid percent encoding, NUL injection and short/open-network passwords.

Run the network host parser tests, then compile against pinned ESP-IDF with `./tools/validate.sh --firmware`. Device provisioning, IP/NTP, reconnect and repeated lifecycle checks remain pending authorization.

## 3. UI, storage and delivery

- [x] Replace main/main.c startup and main/CMakeLists.txt application sources. Keep the baseline demo sources as references, but remove them from this application's startup/build.
- [x] Implement coffee_ui.h/.c: warm paper/coffee palette, dashboard, dotted month calendar, day list, record confirmation, recipe cards/parameters, timer and network/date settings. Battery stays top-right with unavailable fallback. Long OK returns/cancels; all pages show local key hints.
- [x] Use one worker for input, storage and redraw; button callbacks enqueue only. Poll battery outside the LVGL lock. Redraw only on state/time changes and dim idle display with first-key wake consumption.
- [x] Generate a licensed, reproducible 14/16/20 px Chinese subset from every application string; enable UTF-8 and placeholders. Add host LVGL rendering, active-font coverage, a known-missing glyph, text bounds and repeated-navigation memory checks.
- [x] Extend tools/validate.sh with application host tests and headless rendering. Keep generated binaries/PNG/debug artifacts in ignored build/.
- [x] Write paired docs/apps/coffeenotes.md and .zh_CN.md, assets attribution and application index links. Run the complete gate and verify the exact content-addressed firmware archive.

Run: `./tools/validate.sh` and `python3 tools/archive_firmware.py verify build/firmware/<actual-full-image-sha256>`.
Expected: host tests, LVGL audit, firmware layout and archive identity pass. Deliver the merged full image at 0x0 with matching ELF/MAP, report Build/Host tests/Device tests/Unverified, then ask whether to flash. Do not burn without confirmation.
