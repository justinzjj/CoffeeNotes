[简体中文](coffeenotes.zh_CN.md) · **English**

# CoffeeNotes

CoffeeNotes is a Chinese coffee journal and pour-over reference for AI Passport.
It uses the three physical buttons, the portrait 240 x 320 display and the existing BSP.
The application has its own paper/coffee-color UI. No hardware-test menu is used.

## Features and controls

The home screen shows the selected current date, today's cup count and four actions.
UP/DOWN selects an action; OK opens it. Hold OK to return or cancel on other pages.
A double click is treated as one selection, not two records.

| Page | Controls and behavior |
| --- | --- |
| Record a cup | UP/DOWN chooses pour-over, espresso, Americano, latte, cold brew or other. OK saves one record. Hold UP to correct its date/time; hold OK cancels. The date must be synchronized or explicitly confirmed first. |
| Calendar | UP/DOWN moves one day. Hold UP/DOWN changes month. A dot marks a day with records. OK opens the day. The week starts Monday. |
| Day records | Latest saved entries are listed first. UP/DOWN scrolls all entries and the add-record row. OK inspects an entry or adds a cup for that date. Deleting needs a separate explicit confirmation; cancel is selected initially. An asterisk marks manually confirmed timestamps. |
| Brew handbook | Three starting recipes: bright pour-over, full-bodied pour-over and iced pour-over. OK opens parameters and cumulative-water stages. UP/DOWN selects start timer or edit parameters. |
| Recipe editing | Select a field with UP/DOWN; OK toggles editing; UP/DOWN adjusts it. Select save and OK to persist. Hold OK discards changes. |
| Brew timer | OK pauses/resumes; UP finishes and opens a pour-over record draft with actual elapsed time. Hold OK cancels without adding a record. The timer uses monotonic time, so network or manual clock changes do not alter elapsed time. |
| Date/time | UP/DOWN selects year/month/day/hour/minute/confirm; OK enters/exits field editing. Confirm applies the manual date or record correction. Hold OK restores the previous draft. |
| Network | OK opens/retries setup; hold OK cancels the temporary hotspot and returns. Forget network has a separate cancel-by-default confirmation and retains coffee data. |

The display dims to 15% after 60 seconds without input, except during a live timer or provisioning.
The first button gesture restores brightness and is consumed. This is display dimming, not sleep.
The battery reading is displayed at top-right; unavailable readings show `--`.

## Wi-Fi and Beijing time

Open date/network settings, then Wi-Fi setup. The device creates a temporary WPA2
hotspot named `CoffeeNotes-xxxx` with a random eight-digit password shown on screen.
Connect the phone to that hotspot and open `http://192.168.4.1` manually. Enter a
2.4 GHz router SSID/password; no companion application is required. Keep the hotspot
connection even if the phone reports no Internet. Automatic captive-portal pop-up is
not implemented. Open routers may use an empty password; WPA2 passwords are 8-63 bytes.

The form uses a session token, a 512-byte body limit and strict decoding. Credentials
are queued to the network worker, tested for an IP on the candidate SSID, then saved.
A wrong password/unavailable router preserves the previous saved network. The page
reports testing separately from verified saved success. The hotspot and HTTP server
stop on success, cancellation or the five-minute deadline. Failed stop/mode changes
retain cleanup ownership for retry, so the AP cannot be silently restarted as a station.

Saved Wi-Fi reconnects with bounded exponential backoff. SNTP uses `pool.ntp.org` and
`ntp.aliyun.com`, with local time `CST-8` (UTC+8). Association alone does not validate the
clock; only a genuine 2020-2099 SNTP result does. The callback carries monotonic event
ordering so an earlier network sync cannot reclassify a later manually set clock.

On cold boot, the last persisted date (or initial build-date draft) is unconfirmed.
Before network sync, the user must manually confirm date/time to save. While powered,
a confirmed manual clock advances normally. After full power loss, confirm again or
wait for SNTP: no backed-up real-time clock is assumed. If the first sync arrives while
an untouched record draft is open, it replaces the unconfirmed draft timestamp.

Credentials are stored locally in a separate `coffee_wifi` NVS namespace; this baseline
does not enable flash/NVS encryption. Passwords are not logged or built into firmware.
No cloud coffee account, voice interaction, bean-name keyboard or external hardware is used.

## References and storage

These are editable starting points, not fixed rules for every coffee bean:

| Recipe | Powder | Hot water | Temperature | Target | Additional ice |
| --- | ---: | ---: | ---: | ---: | ---: |
| Bright | 15 g | 240 ml | 92 C | 2:30 | none |
| Full-bodied | 20 g | 300 ml | 94 C | 3:00 | none |
| Iced | 15 g | 150 ml | 91 C | 2:00 | 100 g |

Adjustable bounds: 5-40 g powder, 50-600 ml hot water, 70-99 C and 60-360 seconds.
Four stages begin at 0%, 20%, 45%, 70% of the target duration and show cumulative
20%, 45%, 75%, 100% of hot water. Iced recipe ice remains 100 g. The actual timer can
continue beyond the target (display capped at 99:59). Finishing does not automatically
save: confirm the resulting cup draft. No beep is generated.

Up to 512 records retain coffee type, date, minute, optional recipe/actual brew duration
and manual-time flag. Capacity refuses new saves until the user deletes an entry.
Existing entries are never silently evicted. The `coffee_notes/journal` blob uses explicit
little-endian fields, version 1 and CRC32. Settings/recipes share this journal; Wi-Fi is separate.
No boot operation erases NVS. Corruption or read failures block writes and retain the
unreadable data for diagnosis. Setter/commit errors block further writes until reboot:
NVS may already contain the new bytes, so restart reloads the actual persisted journal.
The UI reports success only after the save returns success. Navigation and timer ticks
never write Flash. The default partition table is unchanged (24 KB NVS, PHY, factory app).

## Validation and delivery

- Pure host tests cover date boundaries, leap years, calendar counts, capacity, draft cancellation,
  first-sync drafts, persistence codec, recipe editing and monotonic pause/resume.
- Fault tests cover missing/corrupt NVS, read/set/commit errors and all 512 records.
- Network tests cover strict forms, AP-mode cleanup failure and actual callback clock ordering.
- Headless LVGL rendering uses the same UI/fonts, checks active-font coverage and a known-missing
  glyph, label bounds, all pages/error states, a full calendar, 512 cups and 300 screen transitions.
- The 40 KB LVGL pool is budgeted separately from system heap. No audio or Bluetooth stack is initialized.

Activate ESP-IDF 5.5.3 and run `./tools/validate.sh`. Host UI checks run after the firmware
gate downloads pinned LVGL. Focused preview command:

```sh
cmake -S tests/coffee_preview -B build/coffee-preview -G Ninja
cmake --build build/coffee-preview --parallel 4
(cd build/coffee-preview && ./coffee_preview)
```

The gate retains `build/FoloToy-AI-Passport-full.bin` and matching ELF/MAP/manifest in
`build/firmware/<sha256>/`. Verify that exact archive with
`python3 tools/archive_firmware.py verify build/firmware/<sha256>`.
The merged image is intended for offset `0x0`; an app-only image is not.
A merged flash can reset NVS, including both coffee and network settings. Do not flash
without user approval; data preservation requires a compatible segmented workflow.

Build and host rendering do not establish hardware acceptance. Before acceptance, check
Chinese glyphs/clipping/colors and buttons on all pages; Wi-Fi setup on a real phone;
wrong credentials/restore/cancel/timeout; IP/SNTP and offline cold boot; save/delete/reboot;
timer pause/finish across clock changes; idle first-gesture consumption; and runtime
heap/largest block through repeated provisioning/navigation. No firmware was flashed
as part of autonomous development. See the paired [implementation plan](../superpowers/plans/2026-10-06-coffeenotes.md).

## Verified development checkpoint (2026-10-06)

Branch: `feature/coffeenotes`, baseline `33d3d1d93a1125b356b47b6d83a7a60121be801e`.
Changes remain local and uncommitted. The reused CarCard checkout was not modified by this work.

- Build: PASS, ESP-IDF v5.5.3 complete gate, 1,586,288-byte merged image at `0x0`.
- Host tests: PASS, including real-font/label bounds, full-month/512-cup states and 300 UI transitions.
- Device tests: NOT RUN; no flash was performed.
- Unverified: screen/buttons, actual phone provisioning/IP/SNTP/reconnection, offline date confirmation,
  save/delete through power cycles, dim/wake gestures and runtime network heap/stack behavior.

LVGL 40 KB host pool: peak 28,648 bytes, final free 20,888, largest free 14,120.
ESP-IDF size report: static DRAM 188,292 bytes, 133,004 bytes remaining before runtime allocations.
This memory report does not measure a running device under Wi-Fi/HTTP load.

Merged SHA-256: `d546e9552f3cc3f3f052d0512f75602871b9833c3af28a5e2eadfb5ab8a8a7ef`.
Matching ELF SHA-256: `bc5975a8e6f6a057830ff31cb7b81855c03b6dee6edbc8c60c0e2e381c5a8eec`.
Archive: `build/firmware/d546e9552f3cc3f3f052d0512f75602871b9833c3af28a5e2eadfb5ab8a8a7ef/`.
Its manifest binds the full/app/bootloader/partition images, ELF, MAP and flash arguments;
`archive_firmware.py verify` passed. Full gate log: `build/coffeenotes-validation.log`.
Firmware metadata reports `33d3d1d-dirty`; the retained hash-bound ELF is the debugging identity.

## Authorized device test checkpoint (2026-10-06)

The user authorized testing after connecting the device. The exact merged image above was
flashed at `0x0` on `/dev/cu.usbmodem1101`; esptool identified ESP32-C3 revision v1.1
with 8 MB Flash and verified the written data hash. The erased write range was
`0x00000000` through `0x00183FFF`; no whole-chip erase was performed.

- Build: PASS (the previously validated image; no rebuild or firmware changes).
- Host tests: PASS (the prior complete gate); archive integrity rechecked before flashing.
- Device tests: PASS for the checks observed: startup/ELF identity, display/LVGL/button/
  storage/battery initialization, temporary AP startup and re-entry, and phone DHCP
  assignment (`192.168.4.2`). Two bounded 45-second log windows contained no panic,
  watchdog or heap-corruption report. The user confirmed successful phone provisioning
  and a correct displayed date/time, then confirmed that a saved record survived power
  off/on and Wi-Fi/date/time recovered automatically. This is partial device acceptance.
- Unverified: complete visible Chinese rendering, clipping/colors and physical-control acceptance;
  deletion and recipe-parameter persistence;
  timer/pause/finish; idle dimming/wake gesture; sustained radio heap and stack margins.
  Core recording and network flows passed with user observations; the checks listed above
  have not received explicit confirmation.

Startup reported 122,536 free heap bytes and a largest block of 106,496 bytes before
active Wi-Fi allocation. Initial RF calibration was regenerated after the complete
refresh. Reopening the USB console triggered a USB reset; the next boot succeeded.
The browser requested an absent `/favicon.ico` (404), which alone is not a provisioning
failure. Both bounded monitors were closed and the port released. Raw logs stay local:
`build/device-tests/2026-10-06-startup.log`, `2026-10-06-operation.log`, and
`2026-10-06-result.json`.
