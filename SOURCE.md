# Upstream source record

The display protocol in `src/epd7in5b.c` was manually ported from:

- Waveshare e-Paper repository
- Commit `a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc`
- `RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5bc.c`
- `RaspberryPi_JetsonNano/c/examples/EPD_7in5bc_test.c`

Changes made for this port:

- Converted the Arduino C++ class to freestanding C.
- Replaced Arduino GPIO, delay and SPI functions with a minimal nRF51822 HAL.
- Added a 60-second BUSY safety timeout and RTT diagnostic text.
- Added a streamed native calendar renderer without heap or framebuffer.
- Added separate QFAA/xxAA and QFAB/xxAB linker and pin configurations.

The Vietnamese lunar table in `src/lunar_vietnam_table.inc` is generated using
Hồ Ngọc Đức's astronomical algorithm with `timeZone = 7.0`:

- <https://www.xemamlich.uhm.vn/calrules_en.html>
- <https://www.xemamlich.uhm.vn/calrules.html>

The reproducible generator is `tools/generate_vietnamese_lunar_table.py`.
Runtime conversion is integer-only for the nRF51822. No previous Chinese lunar
table, GUI, graphics, BLE implementation, or SDK framework code is linked.

The Can Chi day/month/year formulas in `src/vietnamese_canchi.c` follow the
same calendar rules reference. Their runtime implementation is integer-only.

`src/font_vietnamese.c` contains a 229-glyph subset imported from CrossPoint
Reader's `ubuntu_10_bold.h`, which was generated from Ubuntu Bold at 10 pt in
1-bit mode. The importer preserves CrossPoint's glyph bearings and 12.4 fixed
point advances, then retains printable ASCII and the complete precomposed
Vietnamese alphabet. The 25-row cell preserves marks which extend beyond the
font's nominal 21-pixel line spacing. See `UBUNTU_FONT_LICENSE.md` and
`tools/import_crosspoint_font.py`.

CrossPoint Reader font format references:

- <https://github.com/crosspoint-reader/crosspoint-reader/blob/develop/lib/EpdFont/EpdFontData.h>
- <https://github.com/crosspoint-reader/crosspoint-reader/blob/develop/lib/EpdFont/scripts/fontconvert.py>

`src/font_digits.c` contains native-resolution large and medium date digits
generated from DejaVu Sans Bold. See `FONT_LICENSE.md` and
`tools/generate_digit_fonts.py`.

QFAA BLE time setting uses Nordic Semiconductor S130 v2.0.1 for nRF51. The
unmodified SoftDevice HEX, API headers, CMSIS/device headers, and Nordic license
are stored under `vendor/`. The application uses only direct SVC APIs; it does
not link Nordic SDK libraries. S130 is excluded from QFAB because the combined
image cannot fit that device's 128 KiB flash.
