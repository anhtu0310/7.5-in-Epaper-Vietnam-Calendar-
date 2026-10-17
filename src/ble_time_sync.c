#include "ble_time_sync.h"

#include <stdint.h>

#include "current_time.h"
#include "hal.h"

#ifndef BLE_ENABLE
#define BLE_ENABLE 0
#endif

#if BLE_ENABLE

#include "ble.h"
#include "ble_gap.h"
#include "ble_gatts.h"
#include "ble_hci.h"
#include "nrf_error.h"
#include "nrf_nvic.h"
#include "nrf_sdm.h"
#include "nrf_soc.h"

#ifndef BLE_WINDOW_SECONDS
#define BLE_WINDOW_SECONDS 120U
#endif
#ifndef BLE_APP_RAM_BASE
#define BLE_APP_RAM_BASE 0x20002078UL
#endif
#ifndef BLE_TX_POWER_DBM
#define BLE_TX_POWER_DBM (-8)
#endif

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define CLOCK_TASKS_HFCLKSTOP REG32(0x40000004UL)
#define CLOCK_TASKS_LFCLKSTOP REG32(0x4000000CUL)
#define CLOCK_LFCLKSTAT       REG32(0x40000418UL)
#define LFCLK_RUNNING         (1UL << 16U)
#define LFCLK_STOP_TIMEOUT    1000000UL
#define RTC1_TASKS_START     REG32(0x40011000UL)
#define RTC1_TASKS_STOP      REG32(0x40011004UL)
#define RTC1_TASKS_CLEAR     REG32(0x40011008UL)
#define RTC1_EVENTS_COMPARE0 REG32(0x40011140UL)
#define RTC1_INTENSET        REG32(0x40011304UL)
#define RTC1_INTENCLR        REG32(0x40011308UL)
#define RTC1_PRESCALER       REG32(0x40011508UL)
#define RTC1_CC0             REG32(0x40011540UL)
#define RTC_COMPARE0_MASK    (1UL << 16U)

#define CURRENT_TIME_SERVICE_UUID 0x1805U
#define CURRENT_TIME_UUID         0x2A2BU
#define ADV_INTERVAL_1_SECOND 1600U
#define EVENT_BUFFER_WORDS 64U

nrf_nvic_state_t nrf_nvic_state = {0};

static volatile bool window_expired;
static volatile bool ble_event_pending;
static uint16_t connection_handle;
static uint16_t time_value_handle;
static bool time_received;
static calendar_date_t received_date;
static uint32_t received_until_midnight;

void RTC1_IRQHandler(void) {
    if (RTC1_EVENTS_COMPARE0 != 0U) {
        RTC1_EVENTS_COMPARE0 = 0U;
        RTC1_TASKS_STOP = 1U;
        window_expired = true;
    }
}

void SWI2_IRQHandler(void) {
    ble_event_pending = true;
}

static void softdevice_fault(uint32_t id, uint32_t pc, uint32_t info) {
    (void)id;
    (void)pc;
    (void)info;
    /* A SoftDevice fault is unrecoverable. Reset instead of continuing corrupt state. */
    REG32(0xE000ED0CUL) = 0x05FA0004UL;
    for (;;) { }
}

static bool check(uint32_t result, const char *operation) {
    if (result == NRF_SUCCESS) return true;
    hal_log("BLE error: ");
    hal_log(operation);
    hal_log_u32(" code: ", result);
    return false;
}

static bool stack_enable(void) {
    /*
     * hal_init() starts LFCLK for its sleep timer. RTC0 is paused or not yet
     * started here, so stop LFCLK and let S130 configure it itself.
     */
    CLOCK_TASKS_LFCLKSTOP = 1U;
    for (uint32_t timeout = LFCLK_STOP_TIMEOUT;
         (CLOCK_LFCLKSTAT & LFCLK_RUNNING) != 0U && timeout != 0U; --timeout) { }

    nrf_clock_lf_cfg_t clock_cfg;
#if defined(LFCLK_SOURCE_xtal)
    clock_cfg.source = NRF_CLOCK_LF_SRC_XTAL;
    clock_cfg.rc_ctiv = 0U;
    clock_cfg.rc_temp_ctiv = 0U;
    clock_cfg.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_20_PPM;
#else
    clock_cfg.source = NRF_CLOCK_LF_SRC_RC;
    clock_cfg.rc_ctiv = 16U;
    clock_cfg.rc_temp_ctiv = 1U;
    clock_cfg.xtal_accuracy = NRF_CLOCK_LF_XTAL_ACCURACY_250_PPM;
#endif
    if (!check(sd_softdevice_enable(&clock_cfg, softdevice_fault), "SoftDevice enable"))
        return false;

    ble_enable_params_t params = {0};
    params.common_enable_params.vs_uuid_count = 0U;
    params.gap_enable_params.periph_conn_count = 1U;
    params.gap_enable_params.central_conn_count = 0U;
    params.gap_enable_params.central_sec_count = 0U;
    params.gatts_enable_params.attr_tab_size = BLE_GATTS_ATTR_TAB_SIZE_DEFAULT;
    params.gatts_enable_params.service_changed = 0U;
    uint32_t ram_base = BLE_APP_RAM_BASE;
    uint32_t enable_result = sd_ble_enable(&params, &ram_base);
    if (enable_result != NRF_SUCCESS) {
        hal_log_u32("BLE configured SoftDevice RAM bytes: ",
                    BLE_APP_RAM_BASE - 0x20000000UL);
        hal_log_u32("BLE required SoftDevice RAM bytes: ",
                    ram_base - 0x20000000UL);
        (void)check(enable_result, "BLE stack enable");
        (void)sd_softdevice_disable();
        CLOCK_TASKS_HFCLKSTOP = 1U;
        return false;
    }
    hal_log_u32("BLE SoftDevice RAM bytes: ", ram_base - 0x20000000UL);
    if (!check(sd_nvic_SetPriority(SD_EVT_IRQn, 3U), "BLE IRQ priority") ||
        !check(sd_nvic_EnableIRQ(SD_EVT_IRQn), "BLE IRQ enable")) {
        (void)sd_softdevice_disable();
        CLOCK_TASKS_HFCLKSTOP = 1U;
        return false;
    }
    return true;
}

static bool gatt_init(const calendar_date_t *date, uint32_t seconds_until_midnight) {
    static const uint8_t name[] = "VNCalendar";
    ble_gap_conn_sec_mode_t open_mode;
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&open_mode);
    if (!check(sd_ble_gap_device_name_set(&open_mode, name, sizeof(name) - 1U),
               "device name")) return false;
    if (!check(sd_ble_gap_tx_power_set((int8_t)BLE_TX_POWER_DBM),
               "BLE transmit power")) return false;

    ble_uuid_t service_uuid = {CURRENT_TIME_SERVICE_UUID, BLE_UUID_TYPE_BLE};
    uint16_t service_handle;
    if (!check(sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &service_uuid,
                                         &service_handle), "time service")) return false;

    ble_gatts_char_md_t char_md = {0};
    char_md.char_props.read = 1U;
    char_md.char_props.write = 1U;
    char_md.char_props.notify = 1U;

    ble_gatts_attr_md_t cccd_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.write_perm);
    cccd_md.vloc = BLE_GATTS_VLOC_STACK;
    char_md.p_cccd_md = &cccd_md;

    ble_gatts_attr_md_t attr_md = {0};
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&attr_md.write_perm);
    attr_md.vloc = BLE_GATTS_VLOC_STACK;

    ble_uuid_t char_uuid = {CURRENT_TIME_UUID, BLE_UUID_TYPE_BLE};
    uint8_t initial_value[CTS_CURRENT_TIME_LENGTH];
    current_time_encode(date, seconds_until_midnight, initial_value);
    ble_gatts_attr_t attr = {0};
    attr.p_uuid = &char_uuid;
    attr.p_attr_md = &attr_md;
    attr.init_len = sizeof(initial_value);
    attr.max_len = sizeof(initial_value);
    attr.p_value = initial_value;

    ble_gatts_char_handles_t handles;
    if (!check(sd_ble_gatts_characteristic_add(service_handle, &char_md, &attr, &handles),
               "time characteristic")) return false;
    time_value_handle = handles.value_handle;
    return true;
}

static bool advertising_start(void) {
    static const uint8_t advertising_data[] = {
        2U, BLE_GAP_AD_TYPE_FLAGS, BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        3U, BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE,
        (uint8_t)(CURRENT_TIME_SERVICE_UUID & 0xFFU),
        (uint8_t)(CURRENT_TIME_SERVICE_UUID >> 8U),
        11U, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME,
        'V', 'N', 'C', 'a', 'l', 'e', 'n', 'd', 'a', 'r'
    };
    if (!check(sd_ble_gap_adv_data_set(advertising_data, sizeof(advertising_data), 0, 0),
               "advertising data")) return false;

    ble_gap_adv_params_t adv = {0};
    adv.type = BLE_GAP_ADV_TYPE_ADV_IND;
    adv.fp = BLE_GAP_ADV_FP_ANY;
    adv.interval = ADV_INTERVAL_1_SECOND;
    adv.timeout = BLE_WINDOW_SECONDS;
    return check(sd_ble_gap_adv_start(&adv), "advertising start");
}

static void window_timer_start(void) {
    window_expired = false;
    RTC1_TASKS_STOP = 1U;
    RTC1_INTENCLR = 0xFFFFFFFFUL;
    RTC1_EVENTS_COMPARE0 = 0U;
    RTC1_TASKS_CLEAR = 1U;
    RTC1_PRESCALER = 4095U; /* 8 Hz. */
    RTC1_CC0 = BLE_WINDOW_SECONDS * 8U;
    RTC1_INTENSET = RTC_COMPARE0_MASK;
    (void)sd_nvic_SetPriority(RTC1_IRQn, 3U);
    (void)sd_nvic_ClearPendingIRQ(RTC1_IRQn);
    (void)sd_nvic_EnableIRQ(RTC1_IRQn);
    RTC1_TASKS_START = 1U;
}

static void handle_event(const ble_evt_t *event) {
    switch (event->header.evt_id) {
        case BLE_GAP_EVT_CONNECTED:
            connection_handle = event->evt.gap_evt.conn_handle;
            hal_log("BLE connected\n");
            break;
        case BLE_GAP_EVT_DISCONNECTED:
            connection_handle = BLE_CONN_HANDLE_INVALID;
            break;
        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            (void)sd_ble_gatts_sys_attr_set(event->evt.gatts_evt.conn_handle,
                                            0, 0U, 0U);
            break;
        case BLE_GATTS_EVT_WRITE: {
            const ble_gatts_evt_write_t *write = &event->evt.gatts_evt.params.write;
            if ((write->handle == time_value_handle) && (write->offset == 0U)) {
                if (current_time_decode(write->data, write->len, &received_date,
                                        &received_until_midnight)) {
                    uint16_t notify_length = CTS_CURRENT_TIME_LENGTH;
                    ble_gatts_hvx_params_t notification = {0};
                    notification.handle = time_value_handle;
                    notification.type = BLE_GATT_HVX_NOTIFICATION;
                    notification.p_len = &notify_length;
                    notification.p_data = (uint8_t *)write->data;
                    /* NRF_ERROR_INVALID_STATE simply means notifications were not enabled. */
                    (void)sd_ble_gatts_hvx(connection_handle, &notification);
                    time_received = true;
                    hal_log("BLE Current Time accepted\n");
                } else {
                    hal_log("BLE Current Time rejected\n");
                }
            }
            break;
        }
        default:
            break;
    }
}

static void drain_events(void) {
    uint32_t event_words[EVENT_BUFFER_WORDS];
    for (;;) {
        uint16_t length = sizeof(event_words);
        uint32_t result = sd_ble_evt_get((uint8_t *)event_words, &length);
        if (result == NRF_ERROR_NOT_FOUND) break;
        if (result != NRF_SUCCESS) {
            hal_log_u32("BLE event error: ", result);
            break;
        }
        handle_event((const ble_evt_t *)event_words);
    }
    ble_event_pending = false;
}

static void stack_disable(void) {
    RTC1_TASKS_STOP = 1U;
    RTC1_INTENCLR = 0xFFFFFFFFUL;
    (void)sd_nvic_DisableIRQ(RTC1_IRQn);
    if (connection_handle != BLE_CONN_HANDLE_INVALID) {
        (void)sd_ble_gap_disconnect(connection_handle,
                                    BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
        while (connection_handle != BLE_CONN_HANDLE_INVALID) {
            drain_events();
            if (connection_handle != BLE_CONN_HANDLE_INVALID) (void)sd_app_evt_wait();
        }
    } else {
        (void)sd_ble_gap_adv_stop();
    }
    (void)sd_nvic_DisableIRQ(SD_EVT_IRQn);
    (void)sd_nvic_ClearPendingIRQ(SD_EVT_IRQn);
    if (!check(sd_softdevice_disable(), "SoftDevice disable")) return;
    /* Do not let a clock request left by radio shutdown inflate daily sleep current. */
    CLOCK_TASKS_HFCLKSTOP = 1U;
}

static bool run_window(calendar_date_t *date, uint32_t *seconds_until_midnight) {
    hal_log("BLE time-sync window starting (120 seconds)\n");
    connection_handle = BLE_CONN_HANDLE_INVALID;
    time_received = false;
    ble_event_pending = false;

    if (!stack_enable()) return false;
    if (!gatt_init(date, *seconds_until_midnight) || !advertising_start()) {
        stack_disable();
        return false;
    }
    window_timer_start();
    hal_log("Connect to VNCalendar and write the 10-byte CTS Current Time value\n");

    while (!window_expired && !time_received) {
        drain_events();
        if (!window_expired && !time_received && !ble_event_pending) (void)sd_app_evt_wait();
    }
    stack_disable();
    if (!time_received) {
        hal_log("BLE time-sync window closed without a new time\n");
        return false;
    }
    *date = received_date;
    *seconds_until_midnight = received_until_midnight;
    return true;
}

bool ble_time_sync_run(calendar_date_t *date, uint32_t *seconds_until_midnight) {
    if ((date == 0) || (seconds_until_midnight == 0)) return false;
    bool synced = run_window(date, seconds_until_midnight);
    /* S130 normally leaves LFCLK running; restart it if enabling the stack failed. */
    if (!hal_lfclk_start()) hal_log("Warning: LFCLK restart after BLE failed\n");
    return synced;
}

#else

void RTC1_IRQHandler(void) { }
void SWI2_IRQHandler(void) { }

bool ble_time_sync_run(calendar_date_t *date, uint32_t *seconds_until_midnight) {
    (void)date;
    (void)seconds_until_midnight;
    return false;
}

#endif
