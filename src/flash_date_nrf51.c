#include "flash_date.h"

#include <stdint.h>

#ifndef FLASH_JOURNAL_PAGE0
#error "FLASH_JOURNAL_PAGE0 must be supplied by the board configuration"
#endif
#ifndef FLASH_JOURNAL_PAGE1
#error "FLASH_JOURNAL_PAGE1 must be supplied by the board configuration"
#endif

#define REG32(address) (*(volatile uint32_t *)(uintptr_t)(address))
#define NVMC_READY     REG32(0x4001E400UL)
#define NVMC_CONFIG    REG32(0x4001E504UL)
#define NVMC_ERASEPAGE REG32(0x4001E508UL)

#define NVMC_READ  0UL
#define NVMC_WRITE 1UL
#define NVMC_ERASE 2UL
#define PAGE_SIZE 1024UL
#define DATE_MAGIC 0x564E4441UL /* "VNDA" */

typedef struct {
    uint32_t sequence;
    uint32_t packed_date;
    uint32_t checksum;
    uint32_t magic;       /* Written last: commits the record atomically. */
} date_record_t;

#define RECORDS_PER_PAGE (PAGE_SIZE / sizeof(date_record_t))

static void nvmc_wait(void) {
    while (NVMC_READY == 0U) { }
}

static uint32_t pack_date(const calendar_date_t *date) {
    return ((uint32_t)date->year << 16U) |
           ((uint32_t)date->month << 8U) | date->day;
}

static bool unpack_date(uint32_t packed, calendar_date_t *date) {
    date->year = (uint16_t)(packed >> 16U);
    date->month = (uint8_t)(packed >> 8U);
    date->day = (uint8_t)packed;
    return calendar_date_valid(date);
}

static uint32_t record_checksum(uint32_t sequence, uint32_t packed) {
    return DATE_MAGIC ^ sequence ^ packed ^ 0xA55A3CC3UL;
}

static bool record_valid(const date_record_t *record, calendar_date_t *date) {
    if (record->magic != DATE_MAGIC) return false;
    if (record->checksum != record_checksum(record->sequence, record->packed_date)) return false;
    return unpack_date(record->packed_date, date);
}

static bool sequence_newer(uint32_t a, uint32_t b) {
    return (int32_t)(a - b) > 0;
}

typedef struct {
    const date_record_t *record;
    uintptr_t page;
    uint32_t slot;
    calendar_date_t date;
} latest_t;

static latest_t find_latest(void) {
    const uintptr_t pages[2] = {FLASH_JOURNAL_PAGE0, FLASH_JOURNAL_PAGE1};
    latest_t latest = {0, 0U, 0U, {0U, 0U, 0U}};
    for (uint8_t page_index = 0U; page_index < 2U; ++page_index) {
        const date_record_t *records = (const date_record_t *)pages[page_index];
        for (uint32_t slot = 0U; slot < RECORDS_PER_PAGE; ++slot) {
            calendar_date_t date;
            if (!record_valid(&records[slot], &date)) continue;
            if ((latest.record == 0) || sequence_newer(records[slot].sequence,
                                                        latest.record->sequence)) {
                latest.record = &records[slot];
                latest.page = pages[page_index];
                latest.slot = slot;
                latest.date = date;
            }
        }
    }
    return latest;
}

static bool slot_erased(const date_record_t *record) {
    return record->sequence == 0xFFFFFFFFUL && record->packed_date == 0xFFFFFFFFUL &&
           record->checksum == 0xFFFFFFFFUL && record->magic == 0xFFFFFFFFUL;
}

static void erase_page(uintptr_t page) {
    NVMC_CONFIG = NVMC_ERASE;
    nvmc_wait();
    NVMC_ERASEPAGE = (uint32_t)page;
    nvmc_wait();
    NVMC_CONFIG = NVMC_READ;
    nvmc_wait();
}

static void write_word(volatile uint32_t *address, uint32_t value) {
    *address = value;
    nvmc_wait();
}

bool flash_date_load(calendar_date_t *date) {
    if (date == 0) return false;
    latest_t latest = find_latest();
    if (latest.record == 0) return false;
    *date = latest.date;
    return true;
}

bool flash_date_save(const calendar_date_t *date) {
    if (!calendar_date_valid(date)) return false;
    latest_t latest = find_latest();
    uint32_t sequence = (latest.record == 0) ? 1U : latest.record->sequence + 1U;
    uintptr_t page = (latest.record == 0) ? FLASH_JOURNAL_PAGE0 : latest.page;
    uint32_t slot = (latest.record == 0) ? 0U : latest.slot + 1U;

    if (slot >= RECORDS_PER_PAGE ||
        !slot_erased(&((const date_record_t *)page)[slot])) {
        /* Keep the old page intact until the other page has been erased. */
        page = (page == FLASH_JOURNAL_PAGE0) ? FLASH_JOURNAL_PAGE1 : FLASH_JOURNAL_PAGE0;
        erase_page(page);
        slot = 0U;
    }

    uint32_t packed = pack_date(date);
    volatile date_record_t *record = &((volatile date_record_t *)page)[slot];
    NVMC_CONFIG = NVMC_WRITE;
    nvmc_wait();
    write_word(&record->sequence, sequence);
    write_word(&record->packed_date, packed);
    write_word(&record->checksum, record_checksum(sequence, packed));
    write_word(&record->magic, DATE_MAGIC);
    NVMC_CONFIG = NVMC_READ;
    nvmc_wait();
    return record_valid((const date_record_t *)record, &(calendar_date_t){0U, 0U, 0U});
}
