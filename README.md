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
   From the home Record a Cup row, press Up to select statistics; OK cycles today,
   this week and this month. Weeks run Monday through Sunday.
2. Open **Settings → Configure Wi-Fi** and press OK.
   On your phone, join the displayed **CoffeeNotes-xxxx** hotspot with its displayed
   password; stay connected even if the phone reports no internet. Open
   **http://192.168.4.1**, enter your **2.4 GHz Wi-Fi** name and password, and tap
   **Test and Save**. The device is ready when it shows **Beijing time
   synced**. Press OK to retry after failure or hotspot expiry;
   enter another network's details to change networks.
3. Open **Record a Cup**, choose a coffee type, and press OK to save.
   Select a date in **Coffee Calendar** and press OK to view its entries.
   Recording works offline; after power-off without a time sync, confirm the date
   in **Settings → Adjust Date Manually**.
4. In **Brew Handbook**, choose a recipe, adjust its parameters or start
   the timer. OK pauses/resumes; Up finishes, then OK saves the entry. Hold OK to
   cancel. Records survive power-off, and saved Wi-Fi reconnects at startup.

5. **Settings → Palette** offers coffee brown, matcha green, sea-salt blue and
   midnight dark. Up/Down previews, OK saves, and holding OK cancels. Statistics
   scope and palette persist with the journal. Calendar shades indicate 0, 1, 2 or
   3+ cups; the selected date retains its shade with an outline.

The device UI is in Simplified Chinese. Six coffee types, three editable recipe
starting points, and up to 512 records are supported. There is no bean-name keyboard,
cloud account or audible timer alarm. A full record store requires explicit deletion
before adding more entries. After 60 seconds without input the screen turns off; brewing
(including pause) and Wi-Fi setup stay lit. The first button gesture only wakes the screen.

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

The tested version-1.1 image has SHA-256
`f3b1c4b233243175402702dd31af683aa58bd37a39994891590b111c855be428`.
Its embedded build version is `ccad39b-dirty`; use the hash-bound ELF for diagnosis.
This is the exact image accepted during device testing; the later source commit does not alter it.
Binary/debug artifacts and local publisher credentials are excluded from Git.

## Validation and documentation

- Build: **PASS**, complete ESP-IDF gate and merged-image/archive checks.
- Host tests: **PASS**, model/form/storage/network fault tests and actual UI/font rendering.
- Device tests: **Observed checks PASS**: verified segmented flash/startup, plus
  user-confirmed idle screen-off/wake, lit display during running/paused brewing, readable
  palette previews, period switching and palette/period retention after power-off.
  Initial-release network/record observations remain in the historical test record.
- Unverified: all-page visual/control acceptance, populated-device calendar intensity,
  timer record saving, deletion/recipe persistence and sustained network memory.

See the [user guide and test record](docs/apps/coffeenotes.md),
[implementation plan](docs/superpowers/plans/2026-10-06-coffeenotes.md),
[UI refinement plan](docs/superpowers/plans/2026-10-06-coffeenotes-ui.md), and
[upstream documentation index](docs/README.md).

Code is covered by the repository's [MIT license](LICENSE).
The included renamed font subset uses the [SIL Open Font License](assets/fonts/OFL.txt).
Asset provenance is recorded in [assets/README.md](assets/README.md).
