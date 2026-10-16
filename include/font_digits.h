#ifndef FONT_DIGITS_H
#define FONT_DIGITS_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    FONT_DIGITS_LARGE = 0,
    FONT_DIGITS_MEDIUM = 1,
} font_digits_style_t;

typedef struct {
    uint8_t width;
    uint8_t advance;
    uint8_t height;
    uint8_t bytes_per_column;
    const uint8_t *columns;
} font_digit_glyph_t;

const font_digit_glyph_t *font_digits_find(font_digits_style_t style, char digit);
uint16_t font_digits_text_width(const char *text, font_digits_style_t style);
bool font_digits_pixel(const font_digit_glyph_t *glyph, uint8_t x, uint8_t y);

#endif
