#ifndef FONT_VIETNAMESE_H
#define FONT_VIETNAMESE_H

#include <stdbool.h>
#include <stdint.h>

#define FONT_VI_HEIGHT 25U

typedef struct {
    uint16_t codepoint;
    uint8_t width;
    uint8_t advance;
    uint32_t rows[FONT_VI_HEIGHT];
} font_vi_glyph_t;

const font_vi_glyph_t *font_vi_find(uint32_t codepoint);
uint32_t font_vi_utf8_next(const char **cursor);
uint16_t font_vi_text_width(const char *text);
bool font_vi_pixel(const font_vi_glyph_t *glyph, uint8_t x, uint8_t y);

#endif
