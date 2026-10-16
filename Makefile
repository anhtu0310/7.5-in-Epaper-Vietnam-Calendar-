BOARD ?= qfaa
CROSS_COMPILE ?= arm-none-eabi-
CC      := $(CROSS_COMPILE)gcc
OBJCOPY := $(CROSS_COMPILE)objcopy
SIZE    := $(CROSS_COMPILE)size
PYOCD   ?= pyocd
EPD_USE_HARDWARE_SPI ?= 1
LFCLK_SOURCE ?= xtal
CALENDAR_START_YEAR ?= 2026
CALENDAR_START_MONTH ?= 9
CALENDAR_START_DAY ?= 2
UPCOMING_DAYS ?= 7
WAKE_INTERVAL_SECONDS ?= 86400
CALENDAR_ROTATION ?= 270
BLE_BUTTON_PIN ?= 2
BLE_BUTTON_HOLD_MS ?= 3000
BLE_WINDOW_SECONDS ?= 120
BLE_TX_POWER_DBM ?= -8
BATTERY_LOW_MV ?= 2300
BATTERY_CRITICAL_MV ?= 2100
BATTERY_RECOVERY_MV ?= 100
BATTERY_CAL_NUMERATOR_MV ?= 3600

ifeq ($(BOARD),qfaa)
BLE_ENABLE ?= 1
EPD_MOSI_PIN ?= 10
EPD_SCLK_PIN ?= 9
EPD_CS_PIN   ?= 8
EPD_DC_PIN   ?= 7
EPD_RST_PIN  ?= 6
EPD_BUSY_PIN ?= 5
EPD_BS_PIN   ?= 4
FLASH_JOURNAL_PAGE0 := 0x0003F800
FLASH_JOURNAL_PAGE1 := 0x0003FC00
ifeq ($(BLE_ENABLE),1)
LINKER_SCRIPT := linker/nrf51822_qfaa_s130.ld
else
LINKER_SCRIPT := linker/nrf51822_qfaa.ld
endif
else ifeq ($(BOARD),qfab)
BLE_ENABLE ?= 0
EPD_MOSI_PIN ?= 5
EPD_SCLK_PIN ?= 8
EPD_CS_PIN   ?= 9
EPD_DC_PIN   ?= 10
EPD_RST_PIN  ?= 11
EPD_BUSY_PIN ?= 12
EPD_BS_PIN   ?= 13
LINKER_SCRIPT := linker/nrf51822_qfab.ld
FLASH_JOURNAL_PAGE0 := 0x0001F800
FLASH_JOURNAL_PAGE1 := 0x0001FC00
else
$(error BOARD must be qfaa or qfab)
endif

ifeq ($(BOARD):$(BLE_ENABLE),qfab:1)
$(error S130 plus this calendar does not fit QFAB's 128 KiB flash; use BLE_ENABLE=0)
endif

BUILD_DIR := build/$(BOARD)
TARGET    := $(BUILD_DIR)/epd7in5b_$(BOARD)

CPPFLAGS := -Iinclude \
  -DEPD_MOSI_PIN=$(EPD_MOSI_PIN) -DEPD_SCLK_PIN=$(EPD_SCLK_PIN) \
  -DEPD_CS_PIN=$(EPD_CS_PIN) -DEPD_DC_PIN=$(EPD_DC_PIN) \
  -DEPD_RST_PIN=$(EPD_RST_PIN) -DEPD_BUSY_PIN=$(EPD_BUSY_PIN) \
  -DEPD_BS_PIN=$(EPD_BS_PIN) -DEPD_USE_HARDWARE_SPI=$(EPD_USE_HARDWARE_SPI) \
  -DLFCLK_SOURCE_$(LFCLK_SOURCE)=1 \
  -DCALENDAR_START_YEAR=$(CALENDAR_START_YEAR) \
  -DCALENDAR_START_MONTH=$(CALENDAR_START_MONTH) \
  -DCALENDAR_START_DAY=$(CALENDAR_START_DAY) -DUPCOMING_DAYS=$(UPCOMING_DAYS) \
  -DWAKE_INTERVAL_SECONDS=$(WAKE_INTERVAL_SECONDS) \
  -DCALENDAR_ROTATION=$(CALENDAR_ROTATION) -DBLE_ENABLE=$(BLE_ENABLE) \
  -DBLE_BUTTON_PIN=$(BLE_BUTTON_PIN) -DBLE_BUTTON_HOLD_MS=$(BLE_BUTTON_HOLD_MS) \
  -DBLE_WINDOW_SECONDS=$(BLE_WINDOW_SECONDS) \
  -DBLE_TX_POWER_DBM=$(BLE_TX_POWER_DBM) \
  -DBATTERY_LOW_MV=$(BATTERY_LOW_MV) -DBATTERY_CRITICAL_MV=$(BATTERY_CRITICAL_MV) \
  -DBATTERY_RECOVERY_MV=$(BATTERY_RECOVERY_MV) \
  -DBATTERY_CAL_NUMERATOR_MV=$(BATTERY_CAL_NUMERATOR_MV) \
  -DFLASH_JOURNAL_PAGE0=$(FLASH_JOURNAL_PAGE0) \
  -DFLASH_JOURNAL_PAGE1=$(FLASH_JOURNAL_PAGE1)
ifeq ($(BLE_ENABLE),1)
CPPFLAGS += -Ivendor/s130/headers -Ivendor/nordic_device -Ivendor/cmsis \
  -DNRF51 -DS130 -DSOFTDEVICE_PRESENT -DNRF_SD_BLE_API_VERSION=2 \
  -DBLE_APP_RAM_BASE=0x20002078
endif
CFLAGS := -mcpu=cortex-m0 -mthumb -std=c11 -Os -g3 -Wall -Wextra \
  -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns \
  -finput-charset=UTF-8 -fexec-charset=UTF-8 \
  -ffunction-sections -fdata-sections
ASFLAGS := -mcpu=cortex-m0 -mthumb -g3
LDFLAGS := -mcpu=cortex-m0 -mthumb -nostdlib -Wl,--gc-sections \
  -Wl,-Map=$(TARGET).map -T$(LINKER_SCRIPT)

C_SOURCES := src/main.c src/epd7in5b.c src/hal_nrf51.c src/rtt.c \
             src/calendar.c src/lunar.c src/special_days.c \
             src/vietnamese_canchi.c \
             src/font_vietnamese.c src/font_digits.c src/ble_sync_qr.c \
             src/calendar_render.c src/time_source_nrf51.c \
             src/battery_nrf51.c src/button_nrf51.c src/flash_date_nrf51.c \
             src/unix_time.c src/current_time.c src/ble_time_sync.c \
             src/runtime.c
S_SOURCE  := src/startup_nrf51.S
OBJECTS   := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(C_SOURCES)) \
             $(patsubst src/%.S,$(BUILD_DIR)/%.o,$(S_SOURCE))
DEPS      := $(OBJECTS:.o=.d)

.PHONY: all qfaa qfab clean erase flash flash-softdevice reset qfaa-flash qfab-flash test preview show-config

all: $(TARGET).hex $(TARGET).bin

qfaa:
	$(MAKE) BOARD=qfaa all

qfab:
	$(MAKE) BOARD=qfab all

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: src/%.S | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -MMD -MP -c $< -o $@

$(TARGET).elf: $(OBJECTS) $(LINKER_SCRIPT) linker/sections.ld
	$(CC) $(LDFLAGS) $(OBJECTS) -lgcc -o $@
	$(SIZE) $@

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex $< $@

$(TARGET).bin: $(TARGET).elf
	$(OBJCOPY) -O binary $< $@

show-config:
	@echo BOARD=$(BOARD)
	@echo MOSI=$(EPD_MOSI_PIN) SCLK=$(EPD_SCLK_PIN) CS=$(EPD_CS_PIN) DC=$(EPD_DC_PIN)
	@echo RST=$(EPD_RST_PIN) BUSY=$(EPD_BUSY_PIN) BS=$(EPD_BS_PIN)
	@echo EPD_USE_HARDWARE_SPI=$(EPD_USE_HARDWARE_SPI)
	@echo LFCLK_SOURCE=$(LFCLK_SOURCE)
	@echo CALENDAR_START=$(CALENDAR_START_YEAR)-$(CALENDAR_START_MONTH)-$(CALENDAR_START_DAY)
	@echo UPCOMING_DAYS=$(UPCOMING_DAYS)
	@echo WAKE_INTERVAL_SECONDS=$(WAKE_INTERVAL_SECONDS)
	@echo CALENDAR_ROTATION=$(CALENDAR_ROTATION)
	@echo BLE_ENABLE=$(BLE_ENABLE) BLE_BUTTON_PIN=$(BLE_BUTTON_PIN) BLE_WINDOW_SECONDS=$(BLE_WINDOW_SECONDS)
	@echo BLE_TX_POWER_DBM=$(BLE_TX_POWER_DBM)
	@echo BATTERY_LOW_MV=$(BATTERY_LOW_MV) BATTERY_CRITICAL_MV=$(BATTERY_CRITICAL_MV)
	@echo BATTERY_CAL_NUMERATOR_MV=$(BATTERY_CAL_NUMERATOR_MV)
	@echo FLASH_JOURNAL=$(FLASH_JOURNAL_PAGE0),$(FLASH_JOURNAL_PAGE1)
	@echo LINKER_SCRIPT=$(LINKER_SCRIPT)

erase:
	$(PYOCD) erase --mass -t nrf51822

flash: $(TARGET).hex
	$(PYOCD) flash -t nrf51822 --erase sector $<

flash-softdevice:
	$(PYOCD) flash -t nrf51822 --erase sector vendor/s130/hex/s130_nrf51_2.0.1_softdevice.hex

reset:
	$(PYOCD) reset -t nrf51822

qfaa-flash:
	$(MAKE) BOARD=qfaa BLE_ENABLE=$(BLE_ENABLE) erase
ifeq ($(BLE_ENABLE),1)
	$(MAKE) BOARD=qfaa BLE_ENABLE=1 flash-softdevice
endif
	$(MAKE) BOARD=qfaa BLE_ENABLE=$(BLE_ENABLE) flash
	$(MAKE) BOARD=qfaa BLE_ENABLE=$(BLE_ENABLE) reset

qfab-flash:
	$(MAKE) BOARD=qfab erase
	$(MAKE) BOARD=qfab flash
	$(MAKE) BOARD=qfab reset

test:
	$(HOSTCC) -Iinclude -DEPD_DC_PIN=6 -DEPD_RST_PIN=7 -DEPD_BUSY_PIN=8 \
	  -std=c11 -Wall -Wextra -Werror src/epd7in5b.c tests/test_protocol.c -o tests/test_protocol
	./tests/test_protocol
	$(HOSTCC) -Iinclude -DUPCOMING_DAYS=4 -std=c11 -Wall -Wextra -Werror \
	  -ffreestanding -fno-builtin -fno-tree-loop-distribute-patterns \
	  src/calendar.c src/lunar.c src/special_days.c src/vietnamese_canchi.c \
	  src/font_vietnamese.c src/font_digits.c src/ble_sync_qr.c \
	  src/calendar_render.c src/runtime.c tests/test_calendar.c -o tests/test_calendar
	./tests/test_calendar
	$(HOSTCC) -Iinclude -DBATTERY_LOW_MV=$(BATTERY_LOW_MV) \
	  -DBATTERY_CRITICAL_MV=$(BATTERY_CRITICAL_MV) \
	  -DBATTERY_RECOVERY_MV=$(BATTERY_RECOVERY_MV) \
	  -std=c11 -Wall -Wextra -Werror src/calendar.c src/unix_time.c src/current_time.c \
	  src/battery_nrf51.c tests/test_power_time.c -o tests/test_power_time
	./tests/test_power_time

preview:
	mkdir -p build
	$(HOSTCC) -Iinclude -DUPCOMING_DAYS=$(UPCOMING_DAYS) \
	  -DCALENDAR_ROTATION=$(CALENDAR_ROTATION) -std=c11 -Wall -Wextra -Werror \
	  src/calendar.c src/lunar.c src/special_days.c src/vietnamese_canchi.c \
	  src/font_vietnamese.c src/font_digits.c src/ble_sync_qr.c \
	  src/calendar_render.c src/runtime.c \
	  tools/render_calendar_preview.c -o build/render_calendar_preview
	./build/render_calendar_preview $(CALENDAR_START_YEAR) $(CALENDAR_START_MONTH) \
	  $(CALENDAR_START_DAY) build/calendar-preview.ppm
	PREVIEW_BLE_SYNC=1 ./build/render_calendar_preview $(CALENDAR_START_YEAR) \
	  $(CALENDAR_START_MONTH) $(CALENDAR_START_DAY) build/ble-sync-preview.ppm

clean:
	rm -rf build tests/test_protocol tests/test_calendar tests/test_power_time

HOSTCC ?= gcc
-include $(DEPS)
