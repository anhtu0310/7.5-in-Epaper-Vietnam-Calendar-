#ifndef VIETNAMESE_CANCHI_H
#define VIETNAMESE_CANCHI_H

#include "calendar.h"
#include "lunar.h"

typedef struct {
    const char *stem;
    const char *branch;
} canchi_name_t;

canchi_name_t vietnamese_canchi_day(const calendar_date_t *solar);
canchi_name_t vietnamese_canchi_month(const lunar_date_t *lunar);
canchi_name_t vietnamese_canchi_year(const lunar_date_t *lunar);

#endif
