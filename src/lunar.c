#include "lunar.h"

/*
 * Vietnamese lunisolar calendar for the Hanoi meridian (UTC+7).
 *
 * The compact year data is generated from Hồ Ngọc Đức's astronomical new-moon
 * and principal-term algorithm. Floating-point astronomy is intentionally done
 * offline by tools/generate_vietnamese_lunar_table.py; the nRF51822 executes
 * only the small integer conversion below.
 */

typedef struct {
    uint16_t year;
    uint32_t new_year_jdn;
    uint16_t long_month_bits;
    uint8_t month_count;
    uint8_t leap_month;
} vietnamese_lunar_year_t;

static const vietnamese_lunar_year_t lunar_years[] = {
#include "lunar_vietnam_table.inc"
};

#define FIRST_LUNAR_YEAR 1999U
#define LAST_LUNAR_YEAR  2050U

_Static_assert((sizeof(lunar_years) / sizeof(lunar_years[0])) ==
                   (LAST_LUNAR_YEAR - FIRST_LUNAR_YEAR + 1U),
               "Vietnamese lunar table range mismatch");

static const vietnamese_lunar_year_t *find_lunar_year(uint16_t year) {
    if ((year < FIRST_LUNAR_YEAR) || (year > LAST_LUNAR_YEAR)) return 0;
    const vietnamese_lunar_year_t *entry = &lunar_years[year - FIRST_LUNAR_YEAR];
    return (entry->year == year) ? entry : 0;
}

bool lunar_from_solar(const calendar_date_t *solar, lunar_date_t *lunar) {
    if (!calendar_date_valid(solar) || (lunar == 0)) return false;

    uint32_t jdn = calendar_julian_day(solar);
    uint16_t lunar_year = solar->year;
    const vietnamese_lunar_year_t *year_data = find_lunar_year(lunar_year);
    if ((year_data == 0) || (jdn < year_data->new_year_jdn)) {
        --lunar_year;
        year_data = find_lunar_year(lunar_year);
    }
    if ((year_data == 0) || (jdn < year_data->new_year_jdn)) return false;

    uint32_t offset = jdn - year_data->new_year_jdn;
    uint8_t month = 1U;
    bool leap = false;

    for (uint8_t index = 0U; index < year_data->month_count; ++index) {
        uint8_t length = ((year_data->long_month_bits >> index) & 1U) ? 30U : 29U;
        if (offset < length) {
            lunar->year = lunar_year;
            lunar->month = month;
            lunar->day = (uint8_t)(offset + 1U);
            lunar->leap_month = leap;
            return true;
        }
        offset -= length;

        if ((year_data->leap_month != 0U) && (month == year_data->leap_month) && !leap) {
            leap = true;
        } else {
            leap = false;
            ++month;
        }
    }
    return false;
}
