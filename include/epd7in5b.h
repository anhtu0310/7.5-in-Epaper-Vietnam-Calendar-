#ifndef EPD7IN5B_H
#define EPD7IN5B_H

#include <stdbool.h>
#include <stdint.h>

#define EPD7IN5B_WIDTH       640U
#define EPD7IN5B_HEIGHT      384U
#define EPD7IN5B_FRAME_BYTES ((EPD7IN5B_WIDTH / 2U) * EPD7IN5B_HEIGHT)

typedef enum {
    EPD_COLOR_BLACK = 0x0U,
    EPD_COLOR_WHITE = 0x3U,
    EPD_COLOR_RED = 0x4U,
} epd_color_t;

typedef epd_color_t (*epd_pixel_fn)(uint16_t x, uint16_t y, void *context);

bool epd7in5b_init(void);
bool epd7in5b_clean(void);
bool epd7in5b_display_test_pattern(void);
bool epd7in5b_display_generated(epd_pixel_fn pixel, void *context);
bool epd7in5b_sleep(void);

#endif
