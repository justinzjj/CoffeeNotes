[简体中文](coffeenotes.zh_CN.md) · **English**

# CoffeeNotes

CoffeeNotes is a Chinese coffee journal and pour-over reference for AI Passport.
It uses the three physical buttons, the portrait 240 x 320 display and the existing BSP.
The original interface offers coffee brown, matcha green, sea-salt blue and midnight dark palettes. No hardware-test menu is used.

## Features and controls

The home screen shows today's, this week's or this month's cup count and date range, plus four actions.
Record a Cup is selected initially; press UP to select statistics and OK to cycle and save its scope.
Weeks run Monday through Sunday; changing scope does not alter dates or records.
UP/DOWN selects an action; OK opens it. Hold OK to return or cancel on other pages.
A double click is treated as one selection, not two records.

| Page | Controls and behavior |
| --- | --- |
| Record a cup | UP/DOWN chooses pour-over, espresso, Americano, latte, cold brew or other. OK saves one record. Hold UP to correct its date/time; hold OK cancels. The date must be synchronized or explicitly confirmed first. |
| Calendar | UP/DOWN moves one day. Hold UP/DOWN changes month. Background intensity marks 0, 1, 2 or 3+ cups with a legend; an outline selects a date without replacing its intensity. OK opens the day. The week starts Monday. |
| Day records | Latest saved entries are listed first. UP/DOWN scrolls all entries and the add-record row. OK inspects an entry or adds a cup for that date. Deleting needs a separate explicit confirmation; cancel is selected initially. An asterisk marks manually confirmed timestamps. |
| Brew handbook | Three starting recipes: bright pour-over, full-bodied pour-over and iced pour-over. OK opens parameters and cumulative-water stages. UP/DOWN selects start timer or edit parameters. |
| Recipe editing | Select a field with UP/DOWN; OK toggles editing; UP/DOWN adjusts it. Select save and OK to persist. Hold OK discards changes. |
| Brew timer | OK pauses/resumes; UP finishes and opens a pour-over record draft with actual elapsed time. Hold OK cancels without adding a record. The timer uses monotonic time, so network or manual clock changes do not alter elapsed time. |
| Palette | Open from Settings. UP/DOWN previews four palettes; OK saves, holding OK cancels and restores the previous palette. |
| Date/time | UP/DOWN selects year/month/day/hour/minute/confirm; OK enters/exits field editing. Confirm applies the manual date or record correction. Hold OK restores the previous draft. |
| Network | OK opens/retries setup; hold OK cancels the temporary hotspot and returns. Forget network has a separate cancel-by-default confirmation and retains coffee data. |

After 60 seconds without input, the backlight switches completely off. Brewing, including a paused
timer, and Wi-Fi provisioning keep the display lit. The first button gesture restores 85% brightness
and is consumed in full; it does not save, navigate or cancel. The application and clock continue
running while the screen is off; this is not device sleep.
The battery reading is displayed at top-right; unavailable readings show `--`.

## Wi-Fi and Beijing time

Open Settings, then Wi-Fi setup. The device creates a temporary WPA2
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
little-endian fields, version 1 and CRC32. Settings, recipes and preferences share this journal; Wi-Fi is separate.
No boot operation erases NVS. Corruption or read failures block writes and retain the
unreadable data for diagnosis. Setter/commit errors block further writes until reboot:
NVS may already contain the new bytes, so restart reloads the actual persisted journal.
The UI reports success only after the save returns success. Scope changes and palette confirmation
persist preferences; theme previews, navigation and timer ticks never write Flash. Journal header
bytes 14 and 15 hold scope and palette without changing version-1 layout or CRC. Older zero-filled
headers select today and coffee brown while retaining records and recipes. Invalid preferences
are treated as corrupt data. The default partition table is unchanged (24 KB NVS, PHY, factory app).

## Validation and delivery

- Pure host tests cover date boundaries, leap years, calendar counts, capacity, draft cancellation,
  first-sync drafts, persistence codec, recipe editing and monotonic pause/resume.
- Fault tests cover missing/corrupt NVS, read/set/commit errors and all 512 records.
- Network tests cover strict forms, AP-mode cleanup failure and actual callback clock ordering.
- Headless LVGL rendering uses the same UI/fonts, checks active-font coverage and a known-missing
  glyph, label bounds, all pages/error states, four palettes, three periods, intensity/selection,
  a full calendar, 512 cups and 1,200 screen transitions.
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

## Initial development checkpoint (2026-10-06, historical)

Branch: `feature/coffeenotes`, baseline `33d3d1d93a1125b356b47b6d83a7a60121be801e`.
At this checkpoint, changes were local and uncommitted. The reused CarCard checkout was not modified by this work.

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

## Initial authorized device test checkpoint (2026-10-06, historical)

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

## UI refinement checkpoint (2026-10-06)

Version 1.1 UI work is on `feature/coffeenotes-ui`, based on `ccad39be6cd8bc58993711a800b5c5b12dbe5c3d`.
At this validation checkpoint, changes were local and uncommitted. Calendar intensity replaces dots; the home statistics
card cycles today/week/month. Record-a-Cup removes the synchronized-date success hint, page
footers are shorter, and four palettes preview before confirmation and persist after saving.
The preserved initial device-test evidence above applies to the earlier image.

- Build: PASS, complete `./tools/validate.sh` using ESP-IDF 5.5.3 and tracked defaults;
  merged image 1,588,448 bytes, application 1,522,912 bytes.
- Host tests: PASS, strict model/storage tests including legacy journal compatibility,
  all preference combinations, save failures and week boundaries; actual-font/bounds rendering
  of 14 pages, four palettes, three periods, intensity/selection and 1,200 transitions.
  Separate sanitizer runs passed; all 29,220 supported dates' week boundaries matched an independent calendar.
- Device tests: NOT RUN for this revision; no flashing was performed.
- Unverified: physical palette contrast, Chinese rendering, revised buttons/statistics,
  and preferences through power-off. Previous unverified timer/delete/idle/network-stress checks remain pending.

The fixed font inventory covers 322 glyphs at all three sizes. Completed palette, statistics,
record and picker renders and exact README PNGs were visually inspected. Host LVGL 40 KB
pool peak: 30,328 bytes; final free: 20,904; largest block: 20,584. This is not device heap measurement.
Full log: `build/coffeenotes-ui-validation.log`. Merged SHA-256: `e3dc2d17afdb93f2786880d2ec7bb847869cdeb0fcd0007d76938d08cdc8e8e7`.
Matching ELF SHA-256: `c0e99275becae222f66cc1567a3b91abf2149a1596748e9970e77831cdc968f5`; embedded build metadata: `ccad39b-dirty`.
Verified archive: `build/firmware/e3dc2d17afdb93f2786880d2ec7bb847869cdeb0fcd0007d76938d08cdc8e8e7/`; its matching ELF/MAP and component images are retained.

The partition-table image exactly matches the first tested image. The verified flash targets
are bootloader at `0x0`, partition table at `0x8000`, and application at `0x10000`;
they exclude NVS (`0x9000`–`0xEFFF`) and PHY data (`0xF000`–`0xFFFF`). The merged image
remains suitable for a blank device at `0x0` but pads those gaps and can reset stored data.
An authorized upgrade should use these exact component images without rebuilding or whole-chip erase
to preserve existing coffee records and Wi-Fi. Read-only enumeration found `/dev/cu.usbmodem1101`
(USB VID/PID `303A:1001`); enumeration is not authorization to flash.

## Automatic screen-off follow-up (2026-10-06)

On the same `feature/coffeenotes-ui` branch, idle backlight now becomes 0% after
60 seconds rather than 15%. Active and paused brewing and Wi-Fi provisioning retain
85% brightness; the first complete wake gesture is consumed. No BSP, journal, Wi-Fi
or clock behavior was changed. This image includes all preceding UI refinements.

- Build: PASS, complete ESP-IDF 5.5.3 gate; verified merged image 1,588,448 bytes.
- Host tests: PASS, existing model/storage/network and actual-font UI checks,
  including 14 pages, four palettes and 1,200 transitions.
- Device tests: NOT RUN for the new image.
- Unverified: physical 60-second screen-off, brewing/pause exception, first-gesture wake,
  and the preceding UI revision's physical acceptance checks.

Merged SHA-256: `f3b1c4b233243175402702dd31af683aa58bd37a39994891590b111c855be428`.
Matching ELF SHA-256: `e59a78730ecb1b4cec1462be30a97c3199ea6d3b3a1fcb8cf1c7347230f14364`.
Verified archive: `build/firmware/f3b1c4b233243175402702dd31af683aa58bd37a39994891590b111c855be428/`; embedded metadata remains `ccad39b-dirty`.
Full gate log: `build/coffeenotes-screen-off-validation.log`. Partition-table bytes and
flash offsets still match the initial tested image, permitting an authorized segmented
upgrade that excludes NVS/PHY. Read-only enumeration still finds `/dev/cu.usbmodem1101`.

## Authorized 1.1 device tests (2026-10-06)

The user authorized segmented flashing of the screen-off image above
(`f3b1c4b233243175402702dd31af683aa58bd37a39994891590b111c855be428`)
to `/dev/cu.usbmodem1101`. ESP32-C3 revision v1.1 and 8 MB XMC Flash were identified.
All three component write hashes passed; erase ranges were `0x0`–`0x5FFF`,
`0x8000`–`0x8FFF`, and `0x10000`–`0x183FFF`. NVS/PHY was excluded and no
whole-chip erase was used. The archive was reverified before each write.

The first 95-second startup window matched the expected ELF and initialized display,
LVGL, buttons, battery and storage without a runtime error. The user reported the
65-second idle blanking and first-gesture wake check normal, and confirmed brewing
and paused brewing stayed lit for the requested 65-second checks. These are user
observations, not electrical or current measurements.

The final 40-second startup window confirmed CoffeeNotes 1.1.0, ESP-IDF 5.5.3 and
ELF prefix `e59a78730` matching the archived ELF; no runtime error was reported.
All bounded monitors were closed and the port released.

- Build: PASS; unchanged verified image from the preceding complete gate.
- Host tests: PASS; preceding complete gate, no source rebuild during flashing.
- Device tests: PASS for verified flashing/startup and user-confirmed idle/wake and
  brewing/pause screen behavior, four readable palette previews, home period switching
  and palette/statistics retention after power-off/on, all confirmed by the user.
- Unverified: all-page visual acceptance, calendar intensity with populated device
  data, deletion/recipe persistence, timer record saving and sustained network memory.

The final startup reported 0 entries, ready storage/input, 122,504 free heap bytes and
a largest block of 106,496 bytes before active networking. The first bundled user
check reported records/network/time normal; record count before flashing was not
independently established; NVS retention was checked by write ranges rather than a
byte-for-byte comparison. Raw logs and the structured test receipt
remain local in `build/device-tests/2026-10-06-ui-*.log` and `2026-10-06-ui-result.json`.
