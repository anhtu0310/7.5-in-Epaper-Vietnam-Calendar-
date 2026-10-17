# Vietnamese calendar for nRF51822 + Waveshare epd7in5b

Standalone ARM-GCC firmware for the 640×384 HINK-E075A01-A8 black/white/red
panel. It retains the known-working Waveshare `EPD_7in5bc.c` V3 sequence and
adds the current portrait Vietnamese calendar UI, hardware SPI, BLE Current Time
sync, flash date recovery, battery monitoring, and once-per-day low-power wake.

There is no bootloader, heap, framebuffer, or newlib dependency. QFAA BLE uses
Nordic S130 v2.0.1; its binary, headers, and license are under `vendor/s130`.

## Pin mappings

| Signal | QFAA | QFAB |
| --- | ---: | ---: |
| MOSI | P0.10 | P0.05 |
| SCLK | P0.09 | P0.08 |
| CS | P0.08 | P0.09 |
| DC | P0.07 | P0.10 |
| RST | P0.06 | P0.11 |
| BUSY | P0.05 | P0.12 |
| BS | P0.04 | P0.13 |
| BLE button | P0.02 | P0.02 (BLE unavailable) |
| Panel flash CS | P0.03 | — |
| Board NOR flash CS / SCK / MOSI | P0.30 / P0.00 / P0.02 | — |
| NFC field detect | P0.25 | — |

Wire the normally-open BLE button between **P0.02 and GND** (`BLE_BUTTON_PIN`).
The internal pull-up is enabled, so no external resistor is required. `BS` is
held low for 4-wire SPI. SPI0 mode 0, MSB first, 2 MHz is the default.

The last three QFAA rows come from the tag's factory firmware. P0.03 selects the
SPI flash on the panel FPC, reached through UC8159 command `0x65`
(FLASH_CONTROL); P0.11 is that flash's data output and is not used yet. The
board NOR flash shares P0.02 with the button; once the flash is in deep
power-down it ignores that line.

## Build and preview

Install `arm-none-eabi-gcc`, GNU Make, and pyOCD, then run:

```sh
make qfaa                 # S130 BLE build (default for QFAA)
make qfab                 # non-BLE build (default for QFAB)
make test
make preview
```

The previews are `build/calendar-preview.ppm` and `build/ble-sync-preview.ppm`.
Build outputs are under `build/qfaa` and `build/qfab`.

Useful configuration examples:

```sh
make BOARD=qfaa CALENDAR_ROTATION=90 all
make BOARD=qfaa EPD_USE_HARDWARE_SPI=0 all
make BOARD=qfaa WAKE_INTERVAL_SECONDS=60 all       # bench test only
make BOARD=qfaa BLE_WINDOW_SECONDS=60 all
```

QFAA has 256 KiB flash and 16 KiB RAM. Its BLE application starts at flash
address `0x1B000`; application RAM starts at `0x20002078`, leaving the lower
8312 bytes for the configured S130 stack. Do not use S130's absolute minimum RAM
address `0x200013C8`: that value does not include this connection and GATT
configuration and causes `sd_ble_enable()` to return `NRF_ERROR_NO_MEM`.
QFAB has only 128 KiB flash, so S130 plus the current calendar/fonts does not
fit; QFAB is intentionally built with `BLE_ENABLE=0`. Both linkers reserve the
final two 1 KiB flash pages for the date journal.

## Flash with DAPLink and pyOCD

The QFAA target mass-erases, flashes S130, flashes the application, and resets:

```sh
make qfaa-flash
```

For QFAB:

```sh
make qfab-flash
```

Read logs with:

```sh
pyocd rtt -t nrf51822
```

Do not flash only the QFAA application after a mass erase: S130 must also be at
the bottom of flash. `qfaa-flash` handles both images in the correct order.

## NFC tap wake

QFAA tags have an NFC field detector on P0.25. It only signals that a reader's
field is present; it carries no data. With `NFC_WAKE=1`, holding an NFC-enabled
phone against the tag opens the same BLE window as a 3-second button hold. A
phone polls about every 0.5 s, so a tap is accepted only after a second pulse
within `NFC_CONFIRM_MS` (2000). Afterwards NFC wake re-arms once the field has
been absent for `NFC_ABSENT_MS` (1500); if a phone keeps lying on the tag for
`NFC_REARM_TIMEOUT_MS` (20000), NFC wake pauses until the next daily refresh.

## BLE time sync

On QFAA, BLE advertises as `VNCalendar` for 120 seconds after power-up. Holding
the button for three seconds opens the same window later. RTC0 is paused while
S130 owns it, then resumed with the remaining daily interval if no sync occurs.
Whenever this window opens, the panel first shows a QR code and Vietnamese
instructions. The QR code opens:

<https://anhtu0310.github.io/VietCalendarTimeSync/>

Open that page in Chrome on Android or a supported desktop computer, press
**Đồng bộ thời gian ngay**, and select `VNCalendar`. If synchronization succeeds,
the calendar is rendered with the new time. If the BLE window expires, the
previous date and remaining RTC interval are kept and the calendar screen is
restored. The QR matrix is stored as a compact 185-byte, error-correction-level-Q
bitmap; it is streamed by the existing column renderer and requires no
framebuffer or runtime QR encoder.

- Current Time Service UUID: `0x1805`
- Current Time characteristic UUID: `0x2A2B`

In nRF Connect, connect to `VNCalendar`, open **Current Time Service**, then
write exactly 10 bytes in hexadecimal to **Current Time**. The value is the
standard CTS local-time layout: year (little endian), month, day, hour, minute,
second, weekday (Monday=1), fractions256, and adjust reason. Generate the
current Vietnam-time payload with:

```sh
python3 -c 'import datetime,struct; z=datetime.timezone(datetime.timedelta(hours=7)); t=datetime.datetime.now(z); print(struct.pack("<H8B",t.year,t.month,t.day,t.hour,t.minute,t.second,t.isoweekday(),0,1).hex())'
```

For example, 2026-09-01 18:30:00 (Tuesday) is
`EA 07 09 01 12 1E 00 02 00 01`. The firmware treats CTS as Vietnam local time,
saves the date, refreshes the screen, schedules the first wake at local
midnight, then disconnects and turns S130/radio off. Read, Write, and Notify are
enabled as defined by CTS. The write is intentionally unauthenticated for easy
setup; open the short physical sync window only in a trusted location.

Override the button and window if required:

```sh
make BOARD=qfaa BLE_BUTTON_PIN=14 BLE_BUTTON_HOLD_MS=3000 \
  BLE_WINDOW_SECONDS=120 BLE_TX_POWER_DBM=-8 all
```

The default `-8 dBm` transmit power is sufficient for close-range setup and
reduces radio energy. Use `0` if the enclosure significantly reduces BLE range.
If the website URL changes, update `tools/generate_ble_qr.py`, regenerate the
packed rows, and replace the table in `src/ble_sync_qr.c`.

## Timekeeping and flash recovery

RTC0 runs from LFCLK at 8 Hz in System ON low-power sleep. The default selects
the internal RC oscillator, because unmodified tags have no 32.768 kHz crystal.
It is calibrated against the 16 MHz crystal at startup and at every daily wake
(`LFCLK_RC_CALIBRATE=1`); uncalibrated RC is only ±2%. On a board fitted with
the crystal, select it:

```sh
make BOARD=qfaa LFCLK_SOURCE=xtal all
```

After each successful calendar refresh, the displayed date is appended to a
two-page wear-levelled journal. A record's commit word is written last, and a
new page is safely written while the older page remains valid. At boot, the
newest valid record is used. If none exists, these build values are used:

```sh
make BOARD=qfaa CALENDAR_START_YEAR=2026 \
  CALENDAR_START_MONTH=9 CALENDAR_START_DAY=1 all
```

Only the displayed date is persisted, not elapsed time. If power is absent for
several days, the firmware cannot infer those missing days; perform BLE sync.

## Battery monitoring

When the batteries directly power nRF51 VDD, no ADC pin or divider is needed.
The ADC measures the internal `VDD/3` path against its 1.2 V band-gap. This is
suitable for two AAA cells in series (ensure fresh-cell voltage remains within
the nRF51 supply rating) or equal coin cells in parallel. Never parallel cells
of different type, age, or charge state.

The voltage appears in the lower-right. The defaults are:

- Warning: 2300 mV; label becomes red.
- Critical: 2100 mV; show one low-battery frame, then suppress e-paper refresh.
- Recovery hysteresis: 100 mV; RTC0 continues while refresh is suppressed.

Configure thresholds after measuring the minimum voltage at which the complete
panel board refreshes reliably:

```sh
make BOARD=qfaa BATTERY_LOW_MV=2300 BATTERY_CRITICAL_MV=2100 \
  BATTERY_RECOVERY_MV=100 all
```

To calibrate the displayed result against a multimeter, scale
`BATTERY_CAL_NUMERATOR_MV` from its nominal 3600 value.

## Power behavior

Long-term sleep uses System ON low-power mode so RTC0 and the button wake source
remain available. The firmware now performs the following cleanup before sleep:

- Powers SPI0 down and disconnects it from the panel pins.
- Disconnects the BUSY input while the panel is asleep, preventing floating-input
  current when the panel is absent or unpowered.
- Stops any remaining high-frequency clock request after display or BLE activity.
- Disables and clears the SoftDevice event interrupt before turning S130 off.
- Always executes panel cleanup after an e-paper error or BUSY timeout.
- Saves a synchronized date once, after the successful calendar refresh, rather
  than writing the same date both before and after that refresh.
- Sleeps in System ON on RTC1 during every delay and BUSY poll instead of
  spinning the CPU through the 15–30 s refresh.
- Puts the board NOR flash and the panel FPC flash into deep power-down, as
  the factory firmware does.
- Leaves the panel control lines at the factory firmware's idle levels.

Each step can be switched off for A/B current measurements:

| Option | QFAA default | Effect |
| --- | --- | --- |
| `BOARD_NOR_FLASH` | 1 | Send `0xB9` to the board NOR flash at boot. Use 0 if the flash is removed. |
| `EPD_PANEL_FLASH` | 1 | Wake the panel flash (`0xAB`) before init, park it (`0xB9`) before `POWER_OFF`. |
| `EPD_IDLE_PINS_HIGH` | 1 | 1 = CS/DC/SCLK/MOSI/RST high while asleep (factory); 0 = all low. |
| `EPD_BUSY_GET_STATUS` | 1 | Send `GET_STATUS` before each BUSY sample (Waveshare). The factory firmware does not. |
| `EPD_BUSY_POLL_MS` | 100 | Sleep between BUSY samples. |
| `EPD_SPI_FREQUENCY_KHZ` | 2000 | 125–8000; the log reports frame streaming time. |
| `NFC_WAKE` | 1 | Open the BLE window on an NFC tap (see below). |

QFAB builds default every board-specific option to 0. For a QFAA board whose
NOR flash was removed and crystal fitted:

```sh
make BOARD=qfaa LFCLK_SOURCE=xtal BOARD_NOR_FLASH=0 all
```

The RTT log reports `EPD frame streaming complete, ms`, `EPD BUSY released after
ms` and `EPD session total ms` for each refresh.

For a valid current measurement, disconnect DAPLink/SWD and fully power-cycle the
nRF51; attaching a debugger leaves nRF51 in a higher-current debug state. Measure
after the BLE window and panel refresh have completed. The measured whole-board
current will also include regulator quiescent current and hardware leakage.

## Calendar and fonts

The streamed portrait UI uses a 640-byte column cache rather than a 122,880-byte
framebuffer. It shows Gregorian and Vietnamese lunar dates, Can Chi day/month/
year, a six-row month grid, today's red outline, special-day markers, and an
upcoming-days footer. Edit `src/special_days.c` to change holidays.

The Vietnamese lunar table covers Gregorian years 2000–2050 and uses Hanoi
time (UTC+7), following Hồ Ngọc Đức's astronomical new-moon/principal-term
algorithm. The UI font is a 1-bit CrossPoint Reader Ubuntu-derived bitmap with
a 25-row cell; large dates use separate DejaVu Sans Bold digit bitmaps. See
`UBUNTU_FONT_LICENSE.md` and `FONT_LICENSE.md`.

## Tests

`make test` checks the Waveshare command stream and byte packing, Gregorian and
Vietnamese lunar conversions, Can Chi values, UI markers/rotation, UTF-8 glyphs,
Unix conversion, CTS encoding/validation, and battery thresholds/hysteresis.

## Upstream display source

- <https://github.com/waveshareteam/e-Paper>
- Commit `a794fbc39656b0f93938d1ffb3fdc77eaed9e9fc`
- `RaspberryPi_JetsonNano/c/lib/e-Paper/EPD_7in5bc.c`

The port preserves Waveshare's reset timing, per-byte CS pulse, `GET_STATUS`
BUSY polling, initialization bytes, native color values, and the power-on after
RAM-write order required by this panel.
