#include "special_days.h"

#include "lunar.h"

typedef struct {
    uint8_t month;
    uint8_t day;
    const char *name;
    uint8_t importance;
} recurring_day_t;

/* UTF-8 Vietnamese labels rendered by the embedded Vietnamese bitmap font. */
static const recurring_day_t solar_days[] = {
    {1U, 1U, "Sinh nhật Mẹ Huệ", SPECIAL_MINOR},
    {1U, 24U, "Sinh nhật Ngọc Lùn", SPECIAL_MINOR},
    {3U, 8U, "QUỐC TẾ PHỤ NỮ", SPECIAL_MINOR},
    {4U, 30U, "GIẢI PHÓNG MIỀN NAM", SPECIAL_MINOR},
    {5U, 1U, "QUỐC TẾ LAO ĐỘNG", SPECIAL_MINOR},
    {8U, 22U, "Sinh nhật Chi Xẻng", SPECIAL_MINOR},
    {9U, 2U, "QUỐC KHÁNH", SPECIAL_MINOR},
    {10U, 3U, "Sinh nhật Tú đẹp zai" , SPECIAL_MINOR},
    {10U, 20U, "PHỤ NỮ VIỆT NAM", SPECIAL_MINOR},
    {11U, 20U, "NHÀ GIÁO VIỆT NAM", SPECIAL_MINOR},
    {12U, 12U, "Sinh nhật Bố Vũ", SPECIAL_MINOR},
    {12U, 22U, "QUÂN ĐỘI NHÂN DÂN", SPECIAL_MINOR},
};

static const recurring_day_t lunar_days[] = {
    {1U, 25U, "GIỖ MẸ KHÔI", SPECIAL_MAJOR}, 
    {2U, 20U, "GIỖ BỐ THÀNH", SPECIAL_MAJOR}, 
    {2U, 5U, "GIỖ CHỊ GÁI VÂN", SPECIAL_MAJOR},
    {3U, 6U, "GIỖ EM TRAI HÙNG", SPECIAL_MAJOR}, 
    {3U, 14U, "GIỖ ÔNG PHÚC", SPECIAL_MAJOR},
    {5U, 2U, "GIỖ BÀ HÓT", SPECIAL_MAJOR},
    {7U, 3U, "GIỖ ÔNG NINH", SPECIAL_MAJOR},
    {9U, 10U, "GIỖ CỤ CHÍNH", SPECIAL_MAJOR},
    {10U, 13U, "GIỖ BÀ HỢI", SPECIAL_MAJOR},
    {11U, 27U, "GIỖ ÔNG TÂM", SPECIAL_MAJOR},
    {11U, 30U, "GIỖ BÀ TẺO", SPECIAL_MAJOR},
    {12U, 16U, "GIỖ MẸ HƯNG", SPECIAL_MAJOR},
    {12U, 18U, "GIỖ BỐ LÂM", SPECIAL_MAJOR},
    {12U, 27U, "GIỖ CỤ TRAI", SPECIAL_MAJOR},
    {1U, 1U, "Tết Nguyên Đán", SPECIAL_MINOR},
    {1U, 2U, "Mùng 2 Tết", SPECIAL_MINOR},
    {1U, 3U, "Mùng 3 Tết", SPECIAL_MINOR},
    {1U, 15U, "Tết Nguyên Tiêu", SPECIAL_MINOR},
    {3U, 10U, "GIỖ TỔ HÙNG VƯƠNG", SPECIAL_MINOR},
    {4U, 15U, "Phật Đản", SPECIAL_MINOR},
    {5U, 5U, "Tết Đoan Ngọ", SPECIAL_MINOR},
    {7U, 15U, "Lễ Vu Lan", SPECIAL_MINOR},
    {8U, 15U, "Tết Trung Thu", SPECIAL_MINOR},
    {12U, 23U, "Ông Công Ông Táo", SPECIAL_MINOR},
};

static const recurring_day_t *lookup(const recurring_day_t *table, uint8_t count,
                                     uint8_t month, uint8_t day) {
    for (uint8_t i = 0U; i < count; ++i) {
        if ((table[i].month == month) && (table[i].day == day)) return &table[i];
    }
    return 0;
}

static special_day_match_t empty_match(void) {
    special_day_match_t result = {0};
    return result;
}

special_day_match_t special_day_match_on(const calendar_date_t *date) {
    special_day_match_t result = empty_match();
    const recurring_day_t *event = lookup(
        solar_days, (uint8_t)(sizeof(solar_days) / sizeof(solar_days[0])),
        date->month, date->day);
    if (event != 0) {
        result.name = event->name;
        result.event_month = event->month;
        result.event_day = event->day;
        result.solar_date = *date;
        result.importance = event->importance;
        result.calendar = SPECIAL_SOLAR;
        return result;
    }

    lunar_date_t lunar;
    if (!lunar_from_solar(date, &lunar) || lunar.leap_month) return result;
    event = lookup(lunar_days, (uint8_t)(sizeof(lunar_days) / sizeof(lunar_days[0])),
                   lunar.month, lunar.day);
    if (event != 0) {
        result.name = event->name;
        result.event_month = event->month;
        result.event_day = event->day;
        result.solar_date = *date;
        result.importance = event->importance;
        result.calendar = SPECIAL_LUNAR;
    }
    return result;
}

const char *special_day_on(const calendar_date_t *date) {
    return special_day_match_on(date).name;
}

special_day_match_t special_day_next(const calendar_date_t *today, uint8_t lookahead_days) {
    special_day_match_t result = empty_match();
    (void)special_days_upcoming(today, lookahead_days, &result, 1U);
    return result;
}

uint8_t special_days_upcoming(const calendar_date_t *today, uint8_t lookahead_days,
                              special_day_match_t *matches, uint8_t capacity) {
    if ((matches == 0) || (capacity == 0U)) return 0U;
    uint8_t count = 0U;
    calendar_date_t date = *today;
    for (uint8_t offset = 1U; offset <= lookahead_days; ++offset) {
        calendar_add_days(&date, 1U);
        special_day_match_t match = special_day_match_on(&date);
        if (match.name != 0) {
            match.days_away = offset;
            matches[count++] = match;
            if (count >= capacity) break;
        }
    }
    return count;
}
