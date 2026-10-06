<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Assets

This directory stores reusable fonts, images, music, and sound effects, organized by asset type.

Keep each asset in the matching subdirectory and document its destination, naming, integration method, and source/license. Do not mix binary assets with Markdown documentation.

## Fonts

Store reusable font files and generated font sources in `fonts/`.

CoffeeNotes uses `fonts/CoffeeNotesSansSC-Regular.otf` and generated
`fonts/coffee_font_14.c`, `coffee_font_16.c`, `coffee_font_20.c`.
The renamed subset derives from [Noto Sans CJK SC Regular 2.004](https://github.com/notofonts/noto-cjk/tree/Sans2.004),
copyright Adobe 2014-2021, licensed under the [SIL Open Font License](fonts/OFL.txt).
`coffee_glyphs.txt`/`.h` and `coffee_font_manifest.json` retain the inventory and
font hash. Generate with `python3 tools/generate_coffee_fonts.py --source-font <licensed-full-font.otf> --converter <lv_font_conv>`
using `fonttools` and `lv_font_conv 1.5.3`; the default source is the retained subset.
All three sizes include ASCII and every fixed UI character at 4 bpp, uncompressed,
linked by the main component. UI render tests verify actual fonts and a known-missing
character. This fixed subset does not support arbitrary bean names or router SSIDs;
the device only displays its ASCII setup-hotspot name, while the phone uses browser fonts.

- Use descriptive names that include the family, weight, size, and format when relevant.
- Document the source, license, character range, conversion command, and expected destination.
- Check Flash and internal-RAM impact before adding a font; the ESP32-C3 has no PSRAM.
- Do not commit fonts whose license does not permit redistribution.

## Images

Store reusable source images and generated display assets in `images/`.

CoffeeNotes community images are `images/coffeenotes/cover.png`, `calendar.png`,
and `timer.png` (240 × 320 PNG, portrait 3:4). They are completed headless renders
of this project's actual UI and fonts, using synthetic records from
`tests/coffee_preview/preview.c`; they are not device photographs or personal data.
The rendering test completed successfully before the PNGs were obtained; the exact
final files were visually inspected before community submission. They are used as
the community cover/gallery and the bilingual root README preview. No image crop
or resize was applied. Interface artwork is covered by the repository license;
font attribution remains in the Fonts section above.

| File | Dimensions and format | Use and source |
| --- | --- | --- |
| [`images/home.jpg`](images/home.jpg) | 3840 × 2160, JPEG | Product hero image embedded in both project README files to foreground AI Passport and its open, maker-oriented identity. |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724, PNG RGBA | Optional technical infographic retained as a reference asset; it is no longer used as the homepage hero. Generated for this repository with the built-in image generation tool on 2026-09-17; the six labels and values were checked against the documented hardware contract. |
| [`images/logo-wordmark.png`](images/logo-wordmark.png) | 1648 × 336, PNG RGBA | Transparent black wordmark extracted from the repository's original `images/logo.png`; embedded in both project README files for light backgrounds. |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png) | 1648 × 336, PNG RGBA | White version of the extracted wordmark, used by the README `<picture>` element when GitHub is in dark mode. |

- Use descriptive names and document dimensions, pixel format, conversion steps, and destination.
- Prefer formats suitable for the 240 × 320 RGB565 display and account for Flash and internal RAM.
- Preserve editable sources where licensing permits, and record the source and license.
- Never commit device QR secrets, credentials, or personal data in images.

## Music and sound effects

Store reusable music and sound-effect sources in `music/`.

- Document the source, license, sample rate, bit depth, channels, conversion command, and destination.
- Prefer 16 kHz, 16-bit mono PCM when it matches the current BSP audio path.
- Check Flash and internal-RAM cost before embedding audio; stream or chunk long recordings.
- Do not commit media without redistribution permission.
