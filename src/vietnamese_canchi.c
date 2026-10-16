#include "vietnamese_canchi.h"

static const char *const stems[10] = {
    "Giáp", "Ất", "Bính", "Đinh", "Mậu", "Kỷ", "Canh", "Tân", "Nhâm", "Quý"
};

static const char *const branches[12] = {
    "Tý", "Sửu", "Dần", "Mão", "Thìn", "Tỵ",
    "Ngọ", "Mùi", "Thân", "Dậu", "Tuất", "Hợi"
};

canchi_name_t vietnamese_canchi_day(const calendar_date_t *solar) {
    uint32_t jdn = calendar_julian_day(solar);
    canchi_name_t result = {stems[(jdn + 9U) % 10U], branches[(jdn + 1U) % 12U]};
    return result;
}

canchi_name_t vietnamese_canchi_month(const lunar_date_t *lunar) {
    uint32_t stem = ((uint32_t)lunar->year * 12U + lunar->month + 3U) % 10U;
    uint32_t branch = (lunar->month + 1U) % 12U;
    canchi_name_t result = {stems[stem], branches[branch]};
    return result;
}

canchi_name_t vietnamese_canchi_year(const lunar_date_t *lunar) {
    canchi_name_t result = {stems[(lunar->year + 6U) % 10U],
                            branches[(lunar->year + 8U) % 12U]};
    return result;
}
