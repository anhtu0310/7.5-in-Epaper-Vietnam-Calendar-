#ifndef FLASH_DATE_H
#define FLASH_DATE_H

#include <stdbool.h>

#include "calendar.h"

bool flash_date_load(calendar_date_t *date);
bool flash_date_save(const calendar_date_t *date);

#endif
