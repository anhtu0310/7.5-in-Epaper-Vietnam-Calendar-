#!/usr/bin/env python3
"""Import a CrossPoint generated header or v4 .cpfont into the UI font."""

import argparse
import math
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "src" / "font_vietnamese.c"
DEFAULT_HEADER = ROOT / "include" / "font_vietnamese.h"

CPFONT_MAGIC = b"CPFONT\x00\x00"
CPFONT_VERSION = 4
CPFONT_HEADER_FORMAT = "<8sHHB19s"
CPFONT_TOC_FORMAT = "<B3xIIBhhHHBBBI4x"
CPFONT_GLYPH_FORMAT = "<BBHhhH2xI"
CPFONT_HEADER_SIZE = struct.calcsize(CPFONT_HEADER_FORMAT)
CPFONT_TOC_SIZE = struct.calcsize(CPFONT_TOC_FORMAT)
CPFONT_GLYPH_SIZE = struct.calcsize(CPFONT_GLYPH_FORMAT)
STYLE_IDS = {"regular": 0, "bold": 1, "italic": 2, "bolditalic": 3}
STYLE_NAMES = {value: key for key, value in STYLE_IDS.items()}
MAX_FONT_HEIGHT = 64

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


def parse_header(path: Path):
    text = path.read_text(encoding="utf-8")
    if "mode: 1-bit" not in text:
        raise ValueError(
            "generated .h inputs must be CrossPoint 1-bit fonts; "
            "binary .cpfont v4 inputs are converted from 2-bit automatically"
        )

    bitmap = bytes(
        int(value, 16)
        for value in re.findall(
            r"0x([0-9A-Fa-f]{1,2})\b", array_body(text, "uint8_t", "Bitmaps")
        )
    )

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
    if len(fields) < 8:
        raise ValueError("incomplete CrossPoint EpdFontData")
    interval_count = int(fields[3], 0)
    advance_y = int(fields[4], 0)
    if fields[7].lower() == "true":
        raise ValueError(
            "generated .h inputs must be CrossPoint 1-bit fonts; "
            "binary .cpfont v4 inputs are converted from 2-bit automatically"
        )
    if interval_count > len(intervals):
        raise ValueError("EpdFontData interval count exceeds the interval array")
    return bitmap, glyphs, intervals[:interval_count], advance_y, 1, path.name


def checked_region(data: bytes, offset: int, size: int, description: str) -> bytes:
    if offset < 0 or size < 0 or offset + size > len(data):
        raise ValueError(f"truncated .cpfont {description}")
    return data[offset : offset + size]


def parse_cpfont(path: Path, style_name: str):
    data = path.read_bytes()
    if len(data) < CPFONT_HEADER_SIZE:
        raise ValueError("truncated .cpfont header")
    magic, version, flags, style_count, _ = struct.unpack_from(
        CPFONT_HEADER_FORMAT, data, 0
    )
    if magic != CPFONT_MAGIC:
        raise ValueError("invalid .cpfont magic")
    if version != CPFONT_VERSION:
        raise ValueError(f"unsupported .cpfont version {version}; expected v4")
    if flags != 1:
        raise ValueError(f"unsupported .cpfont v4 flags 0x{flags:04X}")
    if not 1 <= style_count <= 4:
        raise ValueError(f"invalid .cpfont style count {style_count}")
    toc_bytes = style_count * CPFONT_TOC_SIZE
    checked_region(data, CPFONT_HEADER_SIZE, toc_bytes, "style table")

    styles = {}
    for index in range(style_count):
        entry = struct.unpack_from(
            CPFONT_TOC_FORMAT, data, CPFONT_HEADER_SIZE + index * CPFONT_TOC_SIZE
        )
        style_id = entry[0]
        if style_id not in STYLE_NAMES or style_id in styles:
            raise ValueError(f"invalid or duplicate .cpfont style id {style_id}")
        styles[style_id] = entry

    requested_id = STYLE_IDS[style_name]
    if requested_id not in styles:
        available = ", ".join(STYLE_NAMES[value] for value in sorted(styles))
        raise ValueError(
            f".cpfont has no {style_name} style; available styles: {available}"
        )

    entry = styles[requested_id]
    (
        _, interval_count, glyph_count, advance_y, _ascender, _descender,
        kern_left_entries, kern_right_entries, kern_left_classes,
        kern_right_classes, ligature_count, data_offset,
    ) = entry
    first_data_offset = CPFONT_HEADER_SIZE + toc_bytes
    if data_offset < first_data_offset or data_offset >= len(data):
        raise ValueError("invalid .cpfont style data offset")
    later_offsets = [value[11] for value in styles.values() if value[11] > data_offset]
    style_end = min(later_offsets) if later_offsets else len(data)
    if style_end > len(data):
        raise ValueError("invalid .cpfont style end offset")

    interval_size = interval_count * 12
    glyph_size = glyph_count * CPFONT_GLYPH_SIZE
    kern_left_size = kern_left_entries * 3
    kern_right_size = kern_right_entries * 3
    kern_matrix_size = kern_left_classes * kern_right_classes
    ligature_size = ligature_count * 8
    bitmap_offset = (
        data_offset + interval_size + glyph_size + kern_left_size
        + kern_right_size + kern_matrix_size + ligature_size
    )
    if bitmap_offset > style_end:
        raise ValueError(".cpfont style sections overlap or exceed the file")

    intervals = [
        struct.unpack_from("<III", data, data_offset + index * 12)
        for index in range(interval_count)
    ]
    glyph_offset = data_offset + interval_size
    glyphs = [
        struct.unpack_from(CPFONT_GLYPH_FORMAT, data, glyph_offset + index * CPFONT_GLYPH_SIZE)
        for index in range(glyph_count)
    ]
    for first, last, offset in intervals:
        if first > last or offset + last - first >= glyph_count:
            raise ValueError("invalid .cpfont Unicode interval")

    bitmap = checked_region(data, bitmap_offset, style_end - bitmap_offset, "bitmap")
    description = f"{path.name}, .cpfont v4 {style_name}, 2-bit converted to 1-bit"
    return bitmap, glyphs, intervals, advance_y, 2, description


def parse_source(path: Path, style_name: str = "regular"):
    if path.suffix.lower() == ".cpfont":
        return parse_cpfont(path, style_name)
    return parse_header(path)


def source_glyph(codepoint: int, glyphs, intervals):
    for first, last, offset in intervals:
        if first <= codepoint <= last:
            index = offset + codepoint - first
            if index >= len(glyphs):
                raise ValueError(f"bad glyph index for U+{codepoint:04X}")
            return glyphs[index]
    raise ValueError(f"font is missing required character U+{codepoint:04X}")


def bitmap_pixel(bitmap: bytes, data_offset: int, pixel_index: int,
                 bits_per_pixel: int, threshold: int) -> bool:
    if bits_per_pixel == 1:
        value = bitmap[data_offset + pixel_index // 8]
        return (value & (1 << (7 - pixel_index % 8))) != 0
    value = bitmap[data_offset + pixel_index // 4]
    level = (value >> (6 - 2 * (pixel_index % 4))) & 0x03
    return level >= threshold


def render_glyph(codepoint: int, bitmap: bytes, glyphs, intervals,
                 height: int, baseline: int, bits_per_pixel: int, threshold: int):
    width, glyph_height, advance_fp4, left, top, data_length, data_offset = source_glyph(
        codepoint, glyphs, intervals
    )
    expected_length = math.ceil(width * glyph_height * bits_per_pixel / 8)
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
            pixel_index = source_y * width + source_x
            if bitmap_pixel(bitmap, data_offset, pixel_index, bits_per_pixel, threshold):
                output_x = left - origin_left + source_x
                rows[output_y] |= 1 << output_x
    return stored_width, advance, rows


def source_comments(source: Path, description: str):
    lines = [f"/* Imported from CrossPoint Reader: {description}. */"]
    if source.stem.lower().startswith("ubuntu"):
        lines.insert(0, "/* Ubuntu derivative nRF51822 Calendar Bitmap. */")
        lines.append("/* Ubuntu Font Licence 1.0: see UBUNTU_FONT_LICENSE.md. */")
    else:
        lines.append("/* Retain and follow the source font's licence when redistributing. */")
    return lines


def generate(source: Path, style_name: str = "regular", threshold: int = 2):
    bitmap, glyphs, intervals, nominal_height, bits_per_pixel, description = parse_source(
        source, style_name
    )
    required = [source_glyph(codepoint, glyphs, intervals) for codepoint in CODEPOINTS]
    baseline = max(glyph[4] for glyph in required)
    height = baseline + max(glyph[1] - glyph[4] for glyph in required)
    if not 1 <= height <= MAX_FONT_HEIGHT:
        raise ValueError(
            f"required characters need a {height}-row cell; supported range is 1..{MAX_FONT_HEIGHT}"
        )
    lines = source_comments(source, description) + [
        f"/* {nominal_height}-px line spacing; {height}-row cell preserves Vietnamese marks. */",
        '#include "font_vietnamese.h"',
        "",
        "static const font_vi_glyph_t glyphs[] = {",
    ]
    for codepoint in CODEPOINTS:
        width, advance, rows = render_glyph(
            codepoint, bitmap, glyphs, intervals, height, baseline,
            bits_per_pixel, threshold
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
    return "\n".join(lines), height


def update_header_height(path: Path, height: int) -> None:
    text = path.read_text(encoding="utf-8")
    updated, count = re.subn(
        r"(#define\s+FONT_VI_HEIGHT\s+)\d+U", rf"\g<1>{height}U", text, count=1
    )
    if count != 1:
        raise ValueError(f"could not update FONT_VI_HEIGHT in {path}")
    path.write_text(updated, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Import a CrossPoint 1-bit .h or 2-bit v4 .cpfont UI font"
    )
    parser.add_argument("font", type=Path, help="CrossPoint generated .h or v4 .cpfont")
    parser.add_argument(
        "--style", choices=STYLE_IDS, default="regular",
        help="style selected from a multi-style .cpfont (default: regular)",
    )
    parser.add_argument(
        "--threshold", type=int, choices=(1, 2, 3), default=2,
        help="minimum 2-bit coverage kept as ink: 1 light, 2 balanced, 3 dark",
    )
    parser.add_argument(
        "--output", type=Path,
        help=f"generated C file (default: {DEFAULT_OUTPUT})",
    )
    parser.add_argument(
        "--header", type=Path,
        help="font_vietnamese.h to update; automatic when --output is omitted",
    )
    args = parser.parse_args()

    output = args.output if args.output is not None else DEFAULT_OUTPUT
    header = args.header if args.header is not None else (
        DEFAULT_HEADER if args.output is None else None
    )
    generated, height = generate(args.font, args.style, args.threshold)
    output.write_text(generated, encoding="utf-8")
    if header is not None:
        update_header_height(header, height)
    print(
        f"generated {len(CODEPOINTS)} glyphs ({height} rows) "
        f"from {args.font} into {output}"
    )


if __name__ == "__main__":
    main()
