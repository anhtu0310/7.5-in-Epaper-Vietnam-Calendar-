#!/usr/bin/env python3
"""Import a 1-bit CrossPoint EpdFont header into the calendar UI font."""

import argparse
import math
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "src" / "font_vietnamese2.c"

VIETNAMESE = (
    "ÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬĐÈÉẺẼẸÊỀẾỂỄỆÌÍỈĨỊ"
    "ÒÓỎÕỌÔỒỐỔỖỘƠỜỚỞỠỢÙÚỦŨỤƯỪỨỬỮỰỲÝỶỸỴ"
    "àáảãạăằắẳẵặâầấẩẫậđèéẻẽẹêềếểễệìíỉĩị"
    "òóỏõọôồốổỗộơờớởỡợùúủũụưừứửữựỳýỷỹỵ"
)
CODEPOINTS = sorted(set(range(32, 127)) | {ord(ch) for ch in VIETNAMESE})


def array_body(text: str, c_type: str, suffix: str) -> str:
    pattern = (
        rf"static\s+const\s+{re.escape(c_type)}\s+\w+{suffix}"
        rf"(?:\s*\[[^]]*\])?\s*=\s*\{{(.*?)\}};"
    )
    match = re.search(pattern, text, re.S)
    if not match:
        raise ValueError(f"could not find CrossPoint {suffix} array")
    return match.group(1)


def parse_source(path: Path):
    text = path.read_text(encoding="utf-8")
    if "mode: 1-bit" not in text:
        raise ValueError("only CrossPoint 1-bit font headers are supported")

    bitmap = [
        int(value, 16)
        for value in re.findall(r"0x([0-9A-Fa-f]{1,2})\b", array_body(text, "uint8_t", "Bitmaps"))
    ]

    glyphs = []
    glyph_body = array_body(text, "EpdGlyph", "Glyphs")
    for values in re.findall(r"\{\s*([^{}]+?)\s*\}", glyph_body):
        numbers = [int(value.strip(), 0) for value in values.split(",")]
        if len(numbers) != 7:
            raise ValueError("invalid EpdGlyph entry")
        glyphs.append(tuple(numbers))

    intervals = []
    interval_body = array_body(text, "EpdUnicodeInterval", "Intervals")
    for values in re.findall(r"\{\s*([^{}]+?)\s*\}", interval_body):
        numbers = [int(value.strip(), 0) for value in values.split(",")]
        if len(numbers) != 3:
            raise ValueError("invalid EpdUnicodeInterval entry")
        intervals.append(tuple(numbers))

    font_match = re.search(
        r"static\s+const\s+EpdFontData\s+\w+\s*=\s*\{(.*?)\};", text, re.S
    )
    if not font_match:
        raise ValueError("could not find CrossPoint EpdFontData")
    fields = [
        line.split(",", 1)[0].strip()
        for line in font_match.group(1).splitlines()
        if line.strip()
    ]
    interval_count = int(fields[3], 0)
    advance_y = int(fields[4], 0)
    is_2_bit = fields[7].lower() == "true"
    if is_2_bit:
        raise ValueError("only CrossPoint 1-bit font headers are supported")
    if advance_y > 32:
        raise ValueError("font line height exceeds the calendar's 32-row storage")
    return bitmap, glyphs, intervals[:interval_count], advance_y


def source_glyph(codepoint: int, glyphs, intervals):
    for first, last, offset in intervals:
        if first <= codepoint <= last:
            index = offset + codepoint - first
            if index >= len(glyphs):
                raise ValueError(f"bad glyph index for U+{codepoint:04X}")
            return glyphs[index]
    raise ValueError(f"font is missing required character U+{codepoint:04X}")


def render_glyph(codepoint: int, bitmap, glyphs, intervals, height: int, baseline: int):
    width, glyph_height, advance_fp4, left, top, data_length, data_offset = source_glyph(
        codepoint, glyphs, intervals
    )
    expected_length = math.ceil(width * glyph_height / 8)
    if data_length != expected_length or data_offset + data_length > len(bitmap):
        raise ValueError(f"invalid bitmap range for U+{codepoint:04X}")

    advance = (advance_fp4 + 8) >> 4
    origin_left = min(0, left)
    stored_width = max(advance, left + width) - origin_left
    advance = max(advance, stored_width)
    if stored_width > 32:
        raise ValueError(f"U+{codepoint:04X} is {stored_width} pixels wide; maximum is 32")

    rows = [0] * height
    for source_y in range(glyph_height):
        output_y = baseline - top + source_y
        if not 0 <= output_y < height:
            raise ValueError(f"U+{codepoint:04X} exceeds the {height}-pixel vertical cell")
        for source_x in range(width):
            bit_index = source_y * width + source_x
            value = bitmap[data_offset + bit_index // 8]
            if value & (1 << (7 - bit_index % 8)):
                output_x = left - origin_left + source_x
                rows[output_y] |= 1 << output_x
    return stored_width, advance, rows


def generate(source: Path) -> str:
    bitmap, glyphs, intervals, nominal_height = parse_source(source)
    required = [source_glyph(codepoint, glyphs, intervals) for codepoint in CODEPOINTS]
    baseline = max(glyph[4] for glyph in required)
    height = baseline + max(glyph[1] - glyph[4] for glyph in required)
    if height > 32:
        raise ValueError("required characters exceed the calendar's 32-row storage")
    lines = [
        "/* Ubuntu derivative nRF51822 Calendar Bitmap. */",
        "/* Imported from CrossPoint Reader ubuntu_10_bold (Ubuntu Bold, 10 pt, 1-bit). */",
        f"/* {nominal_height}-px line spacing; {height}-row cell preserves Vietnamese marks. */",
        "/* Ubuntu Font Licence 1.0: see UBUNTU_FONT_LICENSE.md. */",
        '#include "font_vietnamese.h"',
        "",
        "static const font_vi_glyph_t glyphs[] = {",
    ]
    for codepoint in CODEPOINTS:
        width, advance, rows = render_glyph(
            codepoint, bitmap, glyphs, intervals, height, baseline
        )
        row_text = ", ".join(f"0x{row:08X}UL" for row in rows)
        lines.append(
            f"    {{0x{codepoint:04X}UL, {width}U, {advance}U, {{{row_text}}}}},"
        )

    lines += [
        "};",
        "",
        "#define GLYPH_COUNT ((uint16_t)(sizeof(glyphs) / sizeof(glyphs[0])))",
        "",
        "const font_vi_glyph_t *font_vi_find(uint32_t codepoint) {",
        "    uint16_t low = 0U;",
        "    uint16_t high = GLYPH_COUNT;",
        "    while (low < high) {",
        "        uint16_t middle = (uint16_t)(low + (high - low) / 2U);",
        "        if ((uint32_t)glyphs[middle].codepoint < codepoint) low = (uint16_t)(middle + 1U);",
        "        else high = middle;",
        "    }",
        "    if ((low < GLYPH_COUNT) && ((uint32_t)glyphs[low].codepoint == codepoint)) return &glyphs[low];",
        "    return font_vi_find((uint32_t)'?');",
        "}",
        "",
        "uint32_t font_vi_utf8_next(const char **cursor) {",
        "    const uint8_t *text = (const uint8_t *)*cursor;",
        "    uint32_t codepoint;",
        "    if (text[0] == 0U) return 0U;",
        "    if (text[0] < 0x80U) { codepoint = text[0]; *cursor += 1; return codepoint; }",
        "    if (((text[0] & 0xE0U) == 0xC0U) && (text[1] != 0U) && ((text[1] & 0xC0U) == 0x80U)) {",
        "        codepoint = ((uint32_t)(text[0] & 0x1FU) << 6U) | (text[1] & 0x3FU);",
        "        *cursor += 2; return codepoint;",
        "    }",
        "    if (((text[0] & 0xF0U) == 0xE0U) && (text[1] != 0U) && (text[2] != 0U) &&",
        "        ((text[1] & 0xC0U) == 0x80U) && ((text[2] & 0xC0U) == 0x80U)) {",
        "        codepoint = ((uint32_t)(text[0] & 0x0FU) << 12U) |",
        "                    ((uint32_t)(text[1] & 0x3FU) << 6U) | (text[2] & 0x3FU);",
        "        *cursor += 3; return codepoint;",
        "    }",
        "    *cursor += 1; return (uint32_t)'?';",
        "}",
        "",
        "uint16_t font_vi_text_width(const char *text) {",
        "    uint16_t width = 0U;",
        "    while (*text != '\\0') {",
        "        const font_vi_glyph_t *glyph = font_vi_find(font_vi_utf8_next(&text));",
        "        width = (uint16_t)(width + glyph->advance);",
        "    }",
        "    return width;",
        "}",
        "",
        "bool font_vi_pixel(const font_vi_glyph_t *glyph, uint8_t x, uint8_t y) {",
        "    if ((glyph == 0) || (x >= glyph->width) || (y >= FONT_VI_HEIGHT)) return false;",
        "    return (glyph->rows[y] & (uint32_t)(1UL << x)) != 0UL;",
        "}",
        "",
    ]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("font_header", type=Path, help="CrossPoint 1-bit generated font header")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()

    generated = generate(args.font_header)
    args.output.write_text(generated, encoding="utf-8")
    print(f"generated {len(CODEPOINTS)} glyphs from {args.font_header} into {args.output}")


if __name__ == "__main__":
    main()
