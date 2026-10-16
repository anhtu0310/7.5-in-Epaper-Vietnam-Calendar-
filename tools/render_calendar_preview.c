#include <stdio.h>
#include <stdlib.h>

#include "calendar_render.h"

static epd_color_t portrait_pixel(uint16_t x, uint16_t y) {
#if !defined(CALENDAR_ROTATION) || CALENDAR_ROTATION == 90
    return calendar_render_pixel(y, (uint16_t)(383U - x), 0);
#else
    return calendar_render_pixel((uint16_t)(639U - y), x, 0);
#endif
}

int main(int argc, char **argv) {
    calendar_date_t date = {2026U, 8U, 31U};
    const char *output_path = "build/calendar-preview.ppm";
    if (argc == 5) {
        date.year = (uint16_t)strtoul(argv[1], 0, 10);
        date.month = (uint8_t)strtoul(argv[2], 0, 10);
        date.day = (uint8_t)strtoul(argv[3], 0, 10);
        output_path = argv[4];
    } else if (argc != 1) {
        fprintf(stderr, "usage: %s [year month day output.ppm]\n", argv[0]);
        return 2;
    }
    bool prepared;
    if (getenv("PREVIEW_BLE_SYNC") != 0) {
        prepared = calendar_render_prepare_ble_sync();
    } else if (getenv("PREVIEW_LOW_BATTERY") != 0) {
        prepared = calendar_render_prepare_low_battery(2050U);
    } else {
        calendar_render_set_battery(1050U, true);
        prepared = calendar_render_prepare(&date);
    }
    if (!prepared) {
        fprintf(stderr, "invalid or unsupported date\n");
        return 1;
    }

    FILE *output = fopen(output_path, "wb");
    if (output == 0) {
        perror(output_path);
        return 1;
    }
    fprintf(output, "P6\n384 640\n255\n");
    for (uint16_t y = 0U; y < 640U; ++y) {
        for (uint16_t x = 0U; x < 384U; ++x) {
            epd_color_t color = portrait_pixel(x, y);
            const unsigned char rgb[3] = {
                (unsigned char)(color == EPD_COLOR_RED ? 204U :
                                color == EPD_COLOR_BLACK ? 0U : 255U),
                (unsigned char)(color == EPD_COLOR_RED ? 0U :
                                color == EPD_COLOR_BLACK ? 0U : 255U),
                (unsigned char)(color == EPD_COLOR_RED ? 0U :
                                color == EPD_COLOR_BLACK ? 0U : 255U),
            };
            fwrite(rgb, sizeof(rgb), 1U, output);
        }
    }
    return fclose(output) == 0 ? 0 : 1;
}
