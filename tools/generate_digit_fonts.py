#!/usr/bin/env python3
"""Generate smooth, native-resolution date digits for the calendar header."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "src" / "font_digits.c"
FONT_PATH = Path("/media/anhtu/01DB4AC8DC2F3EC0/code/font_helvetica/helveticaneuebold.ttf")
STYLES = (("large", 110), ("medium", 82))
THRESHOLD = 112


def render_glyph(font: ImageFont.FreeTypeFont, character: str):
    left, top, right, bottom = font.getbbox(character)
    width = right - left
    height = bottom - top
    image = Image.new("L", (width, height), 0)
    ImageDraw.Draw(image).text((-left, -top), character, font=font, fill=255)
    pixels = image.load()
    bytes_per_column = math.ceil(height / 8)
    columns = []
    for x in range(width):
        for block in range(bytes_per_column):
            bits = 0
            for bit in range(8):
                y = block * 8 + bit
                if y < height and pixels[x, y] >= THRESHOLD:
                    bits |= 1 << bit
            columns.append(bits)
    advance = max(width, math.ceil(font.getlength(character)))
    return width, advance, height, bytes_per_column, columns


def main():
    lines = [
        "/* Generated from DejaVu Sans Bold at native date sizes. */",
        '#include "font_digits.h"',
        "",
    ]
    style_rows = {}
    for style, size in STYLES:
        font = ImageFont.truetype(str(FONT_PATH), size)
        rows = []
        for digit in "0123456789":
            width, advance, height, stride, columns = render_glyph(font, digit)
            name = f"digit_{style}_{digit}"
            data = ", ".join(f"0x{value:02X}U" for value in columns)
            lines.append(f"static const uint8_t {name}[] = {{{data}}};")
            rows.append((width, advance, height, stride, name))
        style_rows[style] = rows
        lines.append("")

    for style, _ in STYLES:
        lines.append(f"static const font_digit_glyph_t {style}_digits[10] = {{")
        for width, advance, height, stride, name in style_rows[style]:
            lines.append(f"    {{{width}U, {advance}U, {height}U, {stride}U, {name}}},")
        lines += ["};", ""]

    lines += [
        "const font_digit_glyph_t *font_digits_find(font_digits_style_t style, char digit) {",
        "    if ((digit < '0') || (digit > '9')) return 0;",
        "    const font_digit_glyph_t *table =",
        "        (style == FONT_DIGITS_MEDIUM) ? medium_digits : large_digits;",
        "    return &table[(uint8_t)(digit - '0')];",
        "}",
        "",
        "uint16_t font_digits_text_width(const char *text, font_digits_style_t style) {",
        "    uint16_t width = 0U;",
        "    while (*text != '\\0') {",
        "        const font_digit_glyph_t *glyph = font_digits_find(style, *text++);",
        "        if (glyph != 0) width = (uint16_t)(width + glyph->advance);",
        "    }",
        "    return width;",
        "}",
        "",
        "bool font_digits_pixel(const font_digit_glyph_t *glyph, uint8_t x, uint8_t y) {",
        "    if ((glyph == 0) || (x >= glyph->width) || (y >= glyph->height)) return false;",
        "    uint16_t index = (uint16_t)x * glyph->bytes_per_column + (uint16_t)(y >> 3U);",
        "    return (glyph->columns[index] & (uint8_t)(1U << (y & 7U))) != 0U;",
        "}",
        "",
    ]
    OUTPUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"generated native date digits in {OUTPUT}")


if __name__ == "__main__":
    main()
