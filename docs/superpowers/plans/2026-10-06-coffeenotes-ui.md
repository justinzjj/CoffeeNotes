[简体中文](2026-10-06-coffeenotes-ui.zh_CN.md) · **English**

# CoffeeNotes UI refinement implementation plan

> For agentic workers: use superpowers:subagent-driven-development for independent
> model and interface work, then review the integrated change. Track the steps below.

**Goal:** Make coffee activity visible by color intensity, select the home statistics
period, simplify hints, and offer four persistent palettes.

**Architecture:** Keep the existing BSP, networking and journal layout. Store validated
`home_period` and `theme` in journal header bytes 14 and 15, formerly zero-filled;
existing version-1 records retain their values and receive the default preferences.
Only explicit confirmation saves settings. Theme navigation previews without writes.

**Tech stack:** ESP-IDF 5.5.3, existing LVGL 9 renderer, pure C model and licensed CJK subset.

## Model and storage

- [x] Add a failing home-navigation regression, then implement a fifth focus target
  for the statistics card. Default focus remains Record a Cup; Up selects statistics.
- [x] Add `COFFEE_TODAY`, `COFFEE_WEEK`, `COFFEE_MONTH`, `COFFEE_PERIOD_COUNT`,
  `COFFEE_THEME_COUNT = 4`, a `COFFEE_THEMES` page and `COFFEE_ACTION_PREFERENCES`.
  Add `home_period`/`theme` to data and `draft_period`/`draft_theme` to model.
- [x] Implement `coffee_week_start`, `coffee_week_end`, `coffee_week_count` and
  `coffee_period_count`, using Monday–Sunday with independent date-bound clamping.
- [x] Confirming the home card cycles the period transactionally. Settings has Wi-Fi,
  manual date, palette, forget-network; confirming a palette saves, Back cancels.
- [x] Test week/month/year/leap boundaries, old zero-filled journal headers, preference
  validation, round trips, save failure, navigation and preview cancellation.

## Interface

- [x] Replace fixed macros with four palettes: coffee brown, matcha green, sea-salt
  blue and midnight dark. Update persistent screen/header/footer objects as well as pages.
- [x] Calendar uses zero/one/two/three-or-more cup background levels with readable digits;
  selection is an outline so it retains its activity color. Show a small cup-count legend.
- [x] Home reports the chosen period's cup count and date range; the card is selectable.
- [x] Remove the synchronized-date success message from Record a Cup; retain a short
  date-confirmation warning when needed. Shorten all page hints without hiding failures.
- [x] Add a palette selector with whole-screen preview and explicit confirmation.

## Verification and delivery

- [x] Regenerate and verify all fixed Chinese text from the licensed full font.
- [x] Render every page under each palette, 0/1/2/3+ calendar days, all home periods,
  six-week months, 512 cups and repeated theme/navigation changes in the 40 KB LVGL pool.
- [x] Inspect completed renders; run the full gate and verify the exact merged archive.
- [x] Align bilingual user guides and README, retain previous device-test results as
  historical evidence, and mark this revision's device tests NOT RUN.
- [x] Deliver the 0x0 image and palette preview; offer a new authorized device test.
  Do not flash, commit, push or submit a community revision as part of this UI request.

## Idle screen follow-up

- [x] Replace the 15% idle backlight level with 0% after 60 seconds; retain active/paused
  brewing and provisioning exceptions and consume the complete wake gesture.
- [x] Run the full gate again, verify the new merged archive and document pending physical acceptance.
