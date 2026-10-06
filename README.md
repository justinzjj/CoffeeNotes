[简体中文](README.zh_CN.md) · **English**

# CoffeeNotes

A pocket coffee journal and pour-over reference for FoloToy AI Passport.
Log a coffee type with a few button presses, revisit cups by calendar date, and keep
adjustable brewing recipes beside a timer with staged pouring guidance.

![CoffeeNotes interface illustration](assets/images/coffeenotes/cover.png)

The images are completed host renders of the actual interface with sample records,
not device photographs or personal coffee history.

## Start using it

1. Use Up/Down to select, OK to open, and hold OK to go back or cancel.
2. Open **Date & Network → Configure Wi-Fi** and press OK.
   On your phone, join the displayed **CoffeeNotes-xxxx** hotspot with its displayed
   password; stay connected even if the phone reports no internet. Open
   **http://192.168.4.1**, enter your **2.4 GHz Wi-Fi** name and password, and tap
   **Test and Save**. The device is ready when it shows **Beijing time
   synced**. Press OK to retry after failure or hotspot expiry;
   enter another network's details to change networks.
3. Open **Record a Cup**, choose a coffee type, and press OK to save.
   Select a date in **Coffee Calendar** and press OK to view its entries.
   Recording works offline; after power-off without a time sync, confirm the date
   in **Date & Network → Adjust Date Manually**.
4. In **Brew Handbook**, choose a recipe, adjust its parameters or start
   the timer. OK pauses/resumes; Up finishes, then OK saves the entry. Hold OK to
   cancel. Records survive power-off, and saved Wi-Fi reconnects at startup.

The device UI is in Simplified Chinese. Six coffee types, three editable recipe
starting points, and up to 512 records are supported. There is no bean-name keyboard,
cloud account or audible timer alarm. A full record store requires explicit deletion
before adding more entries.

## Build and firmware

This complete project reuses the upstream
[AI Passport BSP](https://gitee.com/FoloToy/ai-passport). Activate **ESP-IDF 5.5.3**
and follow [environment setup](docs/development/engineering/environment-setup.md).

```sh
python3 tools/install_passport_skills.py --install
./tools/validate.sh
```

The gate creates `build/FoloToy-AI-Passport-full.bin`, a merged image for flashing
at **0x0**, and matching ELF/MAP artifacts under `build/firmware/<sha256>/`.
Flashing the merged image may reset existing records and Wi-Fi settings.
The app-only binary must not be flashed at 0x0.

The tested initial image has SHA-256
`d546e9552f3cc3f3f052d0512f75602871b9833c3af28a5e2eadfb5ab8a8a7ef`.
Its embedded build version is `33d3d1d-dirty`; use the hash-bound ELF for diagnosis.
Binary/debug artifacts and local publisher credentials are excluded from Git.

## Validation and documentation

- Build: **PASS**, complete ESP-IDF gate and merged-image/archive checks.
- Host tests: **PASS**, model/form/storage/network fault tests and actual UI/font rendering.
- Device tests: **Core flows PASS**, startup, phone provisioning, correct date/time,
  record retention after power-off, and automatic network/time recovery confirmed.
- Unverified: full visual/control acceptance, timer behavior, deletion and recipe
  persistence, idle dimming/wake behavior, and sustained network memory margins.

See the [user guide and test record](docs/apps/coffeenotes.md),
[implementation plan](docs/superpowers/plans/2026-10-06-coffeenotes.md), and
[upstream documentation index](docs/README.md).

Code is covered by the repository's [MIT license](LICENSE).
The included renamed font subset uses the [SIL Open Font License](assets/fonts/OFL.txt).
Asset provenance is recorded in [assets/README.md](assets/README.md).
