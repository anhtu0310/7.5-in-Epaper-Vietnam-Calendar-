#!/usr/bin/env python3
"""Generate the Vietnamese UTC+7 lunar-year table used by the firmware.

The astronomical formulas and conversion rules follow Hồ Ngọc Đức's
"Thuật toán tính âm lịch": https://www.xemamlich.uhm.vn/calrules.html
"""

from pathlib import Path
from math import floor, pi, sin

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "src" / "lunar_vietnam_table.inc"
TIME_ZONE = 7.0
FIRST_LUNAR_YEAR = 1999
LAST_LUNAR_YEAR = 2050


def jd_from_date(day: int, month: int, year: int) -> int:
    a = (14 - month) // 12
    y = year + 4800 - a
    m = month + 12 * a - 3
    return day + (153 * m + 2) // 5 + 365 * y + y // 4 - y // 100 + y // 400 - 32045


def jd_to_date(jd: int):
    a = jd + 32044
    b = (4 * a + 3) // 146097
    c = a - (b * 146097) // 4
    d = (4 * c + 3) // 1461
    e = c - (1461 * d) // 4
    m = (5 * e + 2) // 153
    day = e - (153 * m + 2) // 5 + 1
    month = m + 3 - 12 * (m // 10)
    year = b * 100 + d - 4800 + m // 10
    return day, month, year


def new_moon_day(k: int) -> int:
    t = k / 1236.85
    t2 = t * t
    t3 = t2 * t
    dr = pi / 180.0
    jd1 = 2415020.75933 + 29.53058868 * k + 0.0001178 * t2 - 0.000000155 * t3
    jd1 += 0.00033 * sin((166.56 + 132.87 * t - 0.009173 * t2) * dr)
    m = 359.2242 + 29.10535608 * k - 0.0000333 * t2 - 0.00000347 * t3
    m_pr = 306.0253 + 385.81691806 * k + 0.0107306 * t2 + 0.00001236 * t3
    f = 21.2964 + 390.67050646 * k - 0.0016528 * t2 - 0.00000239 * t3
    c1 = (0.1734 - 0.000393 * t) * sin(m * dr) + 0.0021 * sin(2.0 * dr * m)
    c1 -= 0.4068 * sin(m_pr * dr)
    c1 += 0.0161 * sin(2.0 * dr * m_pr) - 0.0004 * sin(3.0 * dr * m_pr)
    c1 += 0.0104 * sin(2.0 * dr * f) - 0.0051 * sin(dr * (m + m_pr))
    c1 -= 0.0074 * sin(dr * (m - m_pr))
    c1 += 0.0004 * sin(dr * (2.0 * f + m)) - 0.0004 * sin(dr * (2.0 * f - m))
    c1 -= 0.0006 * sin(dr * (2.0 * f + m_pr))
    c1 += 0.0010 * sin(dr * (2.0 * f - m_pr)) + 0.0005 * sin(dr * (2.0 * m_pr + m))
    if t < -11.0:
        delta_t = 0.001 + 0.000839 * t + 0.0002261 * t2 - 0.00000845 * t3 - 0.000000081 * t * t3
    else:
        delta_t = -0.000278 + 0.000265 * t + 0.000262 * t2
    return floor(jd1 + c1 - delta_t + 0.5 + TIME_ZONE / 24.0)


def sun_longitude(jdn: int) -> int:
    t = (jdn - 2451545.5 - TIME_ZONE / 24.0) / 36525.0
    t2 = t * t
    dr = pi / 180.0
    m = 357.52910 + 35999.05030 * t - 0.0001559 * t2 - 0.00000048 * t * t2
    l0 = 280.46645 + 36000.76983 * t + 0.0003032 * t2
    dl = (1.914600 - 0.004817 * t - 0.000014 * t2) * sin(dr * m)
    dl += (0.019993 - 0.000101 * t) * sin(2.0 * dr * m) + 0.000290 * sin(3.0 * dr * m)
    longitude = (l0 + dl) * dr
    longitude -= 2.0 * pi * floor(longitude / (2.0 * pi))
    return floor(longitude / pi * 6.0)


def lunar_month_11(year: int) -> int:
    offset = jd_from_date(31, 12, year) - 2415021
    k = floor(offset / 29.530588853)
    new_moon = new_moon_day(k)
    if sun_longitude(new_moon) >= 9:
        new_moon = new_moon_day(k - 1)
    return new_moon


def leap_month_offset(a11: int) -> int:
    k = floor((a11 - 2415021.076998695) / 29.530588853 + 0.5)
    last = 0
    i = 1
    arc = sun_longitude(new_moon_day(k + i))
    while True:
        last = arc
        i += 1
        arc = sun_longitude(new_moon_day(k + i))
        if arc == last or i >= 14:
            break
    return i - 1


def solar_to_lunar(day: int, month: int, year: int):
    day_number = jd_from_date(day, month, year)
    k = floor((day_number - 2415021.076998695) / 29.530588853)
    month_start = new_moon_day(k + 1)
    if month_start > day_number:
        month_start = new_moon_day(k)
    a11 = lunar_month_11(year)
    b11 = a11
    if a11 >= month_start:
        lunar_year = year
        a11 = lunar_month_11(year - 1)
    else:
        lunar_year = year + 1
        b11 = lunar_month_11(year + 1)
    lunar_day = day_number - month_start + 1
    diff = floor((month_start - a11) / 29.0)
    lunar_leap = 0
    lunar_month = diff + 11
    if b11 - a11 > 365:
        leap_diff = leap_month_offset(a11)
        if diff >= leap_diff:
            lunar_month = diff + 10
            if diff == leap_diff:
                lunar_leap = 1
    if lunar_month > 12:
        lunar_month -= 12
    if lunar_month >= 11 and diff < 4:
        lunar_year -= 1
    return lunar_day, lunar_month, lunar_year, lunar_leap


def lunar_new_year_jdn(lunar_year: int) -> int:
    start = jd_from_date(1, 1, lunar_year)
    end = jd_from_date(10, 3, lunar_year)
    for jdn in range(start, end + 1):
        day, month, year = jd_to_date(jdn)
        lunar = solar_to_lunar(day, month, year)
        if lunar == (1, 1, lunar_year, 0):
            return jdn
    raise RuntimeError(f"cannot find Vietnamese Lunar New Year {lunar_year}")


def build_year(lunar_year: int):
    first = lunar_new_year_jdn(lunar_year)
    following = lunar_new_year_jdn(lunar_year + 1)
    starts = []
    for jdn in range(first, following + 1):
        day, month, year = jd_to_date(jdn)
        lunar_day, lunar_month, actual_year, leap = solar_to_lunar(day, month, year)
        if lunar_day == 1 and actual_year in (lunar_year, lunar_year + 1):
            starts.append((jdn, lunar_month, leap))
    if starts[0][0] != first or starts[-1][0] != following:
        raise RuntimeError(f"bad boundaries for lunar year {lunar_year}")
    month_count = len(starts) - 1
    if month_count not in (12, 13):
        raise RuntimeError(f"bad month count {month_count} for {lunar_year}")
    lengths = 0
    leap_month = 0
    for index in range(month_count):
        length = starts[index + 1][0] - starts[index][0]
        if length not in (29, 30):
            raise RuntimeError(f"bad month length {length} for {lunar_year}")
        if length == 30:
            lengths |= 1 << index
        if starts[index][2]:
            leap_month = starts[index][1]
    return first, lengths, month_count, leap_month


def main():
    # Published reference cases that distinguish the Hanoi calendar.
    assert solar_to_lunar(21, 1, 1985) == (1, 1, 1985, 0)
    assert solar_to_lunar(17, 2, 2007) == (1, 1, 2007, 0)
    assert solar_to_lunar(2, 2, 2030) == (1, 1, 2030, 0)
    assert solar_to_lunar(21, 3, 2004) == (1, 2, 2004, 1)

    lines = [
        "/* Generated by tools/generate_vietnamese_lunar_table.py.",
        " * Hồ Ngọc Đức astronomical algorithm, Hanoi time zone UTC+7.",
        " * Do not substitute a UTC+8/Chinese lunar table.",
        " */",
    ]
    for year in range(FIRST_LUNAR_YEAR, LAST_LUNAR_YEAR + 1):
        first, lengths, count, leap = build_year(year)
        lines.append(
            f"    {{{year}U, {first}UL, 0x{lengths:04X}U, {count}U, {leap}U}},"
        )
    OUTPUT.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"generated Vietnamese UTC+7 lunar years {FIRST_LUNAR_YEAR}..{LAST_LUNAR_YEAR}")
    for year in (2007, 2030):
        print(year, jd_to_date(lunar_new_year_jdn(year)))


if __name__ == "__main__":
    main()
