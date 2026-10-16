#include "calendar_render.h"

#include "ble_sync_qr.h"
#include "font_digits.h"
#include "font_vietnamese.h"
#include "lunar.h"
#include "special_days.h"
#include "vietnamese_canchi.h"

#ifndef UPCOMING_DAYS
#define UPCOMING_DAYS 4
#endif
#ifndef CALENDAR_ROTATION
#define CALENDAR_ROTATION 90
#endif
#ifndef BLE_WINDOW_SECONDS
#define BLE_WINDOW_SECONDS 120U
#endif
#if (CALENDAR_ROTATION != 90) && (CALENDAR_ROTATION != 270)
#error "CALENDAR_ROTATION must be 90 or 270"
#endif

#define PORTRAIT_WIDTH  384U
#define PORTRAIT_HEIGHT 640U
#define GRID_X           10U
#define GRID_Y           279U
#define CELL_W           52U
#define CELL_H           39U
#define TEXT_ITEMS_MAX   112U
#define UPCOMING_MAX     3U

#define WEEKDAY_BAR_Y   250U
#define WEEKDAY_BAR_H    29U

#define BLE_QR_MODULE_PIXELS  6U
#define BLE_QR_QUIET_MODULES  4U
#define BLE_QR_OUTER_SIZE ((BLE_SYNC_QR_MODULES + 2U * BLE_QR_QUIET_MODULES) * \
                           BLE_QR_MODULE_PIXELS)
#define BLE_QR_X ((PORTRAIT_WIDTH - BLE_QR_OUTER_SIZE) / 2U)
#define BLE_QR_Y 95U

typedef enum {
    RENDER_SCENE_CALENDAR,
    RENDER_SCENE_LOW_BATTERY,
    RENDER_SCENE_BLE_SYNC
} render_scene_mode_t;

typedef struct {
    const char *text;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint8_t scale;
    epd_color_t color;
} text_item_t;

typedef struct {
    const char *text;
    uint16_t x;
    uint16_t y;
    uint16_t width;
    font_digits_style_t style;
    epd_color_t color;
} digit_item_t;

typedef struct {
    text_item_t items[TEXT_ITEMS_MAX];
    digit_item_t digit_items[2];
    uint8_t item_count;
    uint8_t digit_count;
    uint8_t today_cell;
    uint8_t special_cells[42];
    char solar_day[3];
    char lunar_day[3];
    char solar_text[42][3];
    char lunar_text[42][6];
    char weekday[32];
    char solar_month[24];
    char solar_year[8];
    char lunar_month[32];
    char canchi_day[64];
    char canchi_month[64];
    char canchi_year[64];
    char today_special[64];
    char upcoming_label[32];
    char upcoming[UPCOMING_MAX][64];
    char battery_text[16];
    char ble_timeout_text[40];
    text_item_t battery_item;
    bool battery_valid;
    render_scene_mode_t mode;
    uint8_t column_pixels[PORTRAIT_HEIGHT];
    uint16_t cached_x;
    bool column_valid;
} render_scene_t;

static render_scene_t scene;

static char *append_char(char *out, char value) {
    *out++ = value;
    return out;
}

static char *append_string(char *out, const char *value) {
    while (*value != '\0') *out++ = *value++;
    return out;
}

static char *append_u16(char *out, uint16_t value, uint8_t minimum_digits) {
    char digits[5];
    uint8_t count = 0U;
    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));
    while (count < minimum_digits) digits[count++] = '0';
    while (count != 0U) *out++ = digits[--count];
    return out;
}

static void copy_bounded(char *output, uint8_t size, const char *input) {
    uint8_t index = 0U;
    while ((index + 1U < size) && (*input != '\0')) output[index++] = *input++;
    output[index] = '\0';
}

static void add_text(const char *text, uint16_t x, uint16_t y,
                     uint8_t scale, epd_color_t color) {
    if ((text == 0) || (*text == '\0') || (scene.item_count >= TEXT_ITEMS_MAX)) return;
    text_item_t *item = &scene.items[scene.item_count++];
    item->text = text;
    item->x = x;
    item->y = y;
    item->width = font_vi_text_width(text);
    item->scale = scale;
    item->color = color;
}

static void add_text_center(const char *text, uint16_t center_x, uint16_t y,
                            uint8_t scale, epd_color_t color) {
    uint16_t width = (uint16_t)(font_vi_text_width(text) * scale);
    uint16_t x = (width < (uint16_t)(center_x * 2U)) ? (uint16_t)(center_x - width / 2U) : 0U;
    add_text(text, x, y, scale, color);
}

static void set_digit_item(uint8_t index, const char *text, uint16_t center_x,
                           uint16_t y, font_digits_style_t style,
                           epd_color_t color) {
    digit_item_t *item = &scene.digit_items[index];
    item->text = text;
    item->width = font_digits_text_width(text, style);
    item->x = (item->width < center_x * 2U) ?
              (uint16_t)(center_x - item->width / 2U) : 0U;
    item->y = y;
    item->style = style;
    item->color = color;
    if (scene.digit_count <= index) scene.digit_count = (uint8_t)(index + 1U);
}

static void format_battery_text(uint16_t millivolts, bool warning) {
    char *out = scene.battery_text;
    if (warning) out = append_string(out, "PIN YẾU ");
    out = append_u16(out, millivolts, 1U);
    out = append_string(out, " mV");
    *out = '\0';
    scene.battery_item.text = scene.battery_text;
    scene.battery_item.width = font_vi_text_width(scene.battery_text);
    scene.battery_item.scale = 1U;
    scene.battery_item.x = (scene.battery_item.width < 370U) ?
                           (uint16_t)(378U - scene.battery_item.width): 244U;
    // scene.battery_item.x = warning ? 244 : 304;
    scene.battery_item.y = 610U;
    scene.battery_item.color = warning ? EPD_COLOR_RED : EPD_COLOR_BLACK;
}

void calendar_render_set_battery(uint16_t millivolts, bool low_warning) {
    scene.battery_valid = true;
    format_battery_text(millivolts, low_warning);
}

static void format_day(char output[3], uint8_t day) {
    char *out = append_u16(output, day, 1U);
    *out = '\0';
}

static void format_lunar(char output[6], const lunar_date_t *lunar) {
    char *out = output;
    // if (lunar->leap_month) out = append_char(out, 'N');
    if (lunar->day < 10){
    out = append_char(out, ' ');
    out = append_char(out, ' ');
    }
    out = append_u16(out, lunar->day, 1U);
    // out = append_char(out, '/');
    // out = append_u16(out, lunar->month, 1U);
    *out = '\0';
}

static void format_canchi(char *output, const char *prefix, canchi_name_t name) {
    char *out = append_string(output, prefix);
    out = append_string(out, name.stem);
    out = append_char(out, ' ');
    out = append_string(out, name.branch);
    *out = '\0';
}

static void format_upcoming(char output[128], const special_day_match_t *event) {
    char *out = append_string(output, "- ");
    out = append_string(out, event->name);
    out = append_char(out, ' ');
    out = append_u16(out, event->event_day, 2U);
    out = append_char(out, '/');
    out = append_u16(out, event->event_month, 2U);
    if (event->calendar == SPECIAL_LUNAR) out = append_string(out, " (ÂM LỊCH)");
    *out = '\0';
}

static const char *weekday_name(uint8_t weekday) {
    static const char *const names[7] = {
        "CHỦ NHẬT", "THỨ HAI", "THỨ BA", "THỨ TƯ", "THỨ NĂM", "THỨ SÁU", "THỨ BẢY"
    };
    return names[weekday];
}

bool calendar_render_prepare(const calendar_date_t *today) {
    if (!calendar_date_valid(today)) return false;
    scene.item_count = 0U;
    scene.digit_count = 0U;
    scene.mode = RENDER_SCENE_CALENDAR;
    scene.column_valid = false;
    for (uint8_t i = 0U; i < 42U; ++i) scene.special_cells[i] = SPECIAL_NONE;

    lunar_date_t current_lunar;
    if (!lunar_from_solar(today, &current_lunar)) return false;

    copy_bounded(scene.weekday, sizeof(scene.weekday), weekday_name(calendar_weekday(today)));
    char *out = append_string(scene.solar_month, "THÁNG ");
    out = append_u16(out, today->month, 1U);
    *out = '\0';
    out = append_u16(scene.solar_year, today->year, 4U);
    *out = '\0';
    out = append_string(scene.lunar_month, "THÁNG ");
    out = append_u16(out, current_lunar.month, 1U);
    if (current_lunar.leap_month) out = append_string(out, " (NHUẬN)");
    *out = '\0';

    format_canchi(scene.canchi_day, "Ngày ", vietnamese_canchi_day(today));
    format_canchi(scene.canchi_month, "Tháng ", vietnamese_canchi_month(&current_lunar));
    format_canchi(scene.canchi_year, "Năm ", vietnamese_canchi_year(&current_lunar));

    add_text_center(scene.weekday, PORTRAIT_WIDTH / 2U, 12U, 2U, EPD_COLOR_RED);
    format_day(scene.solar_day, today->day);
    format_day(scene.lunar_day, current_lunar.day);
    set_digit_item(0U, scene.solar_day, 96U, 75U,
                   FONT_DIGITS_LARGE, EPD_COLOR_BLACK);
    set_digit_item(1U, scene.lunar_day, 288U, 85U,
                   FONT_DIGITS_MEDIUM, EPD_COLOR_RED);
    add_text_center(scene.solar_month, 96U, 163U, 2U, EPD_COLOR_BLACK);
    add_text_center(scene.solar_year, 96U, 202U, 2U, EPD_COLOR_BLACK);
    add_text_center(scene.lunar_month, 288U, 157U, 1U, EPD_COLOR_BLACK);
    add_text_center(scene.canchi_day, 288U, 180U, 1U, EPD_COLOR_RED);
    add_text_center(scene.canchi_month, 288U, 200U, 1U, EPD_COLOR_BLACK);
    add_text_center(scene.canchi_year, 288U, 220U, 1U, EPD_COLOR_BLACK);

    static const char *const weekdays[7] = {"CN", "Thứ 2", "Thứ 3", "Thứ 4", "Thứ 5", "Thứ 6", "Thứ 7"};
    // for (uint8_t column = 0U; column < 7U; ++column) {
    //     epd_color_t color = (column == 0U) ? EPD_COLOR_RED : EPD_COLOR_BLACK;
    //     add_text_center(weekdays[column], (uint16_t)(GRID_X + column * CELL_W + CELL_W / 2U),
    //                     252U, 1U, color);
    // }
    for (uint8_t column = 0U; column < 7U; ++column) {
        add_text_center(
            weekdays[column],
            (uint16_t)(GRID_X + column * CELL_W + CELL_W / 2U),
            WEEKDAY_BAR_Y,
            1U,
            EPD_COLOR_WHITE
        );
    }

    calendar_date_t first = {today->year, today->month, 1U};
    uint8_t first_weekday = calendar_weekday(&first);
    uint8_t month_days = calendar_days_in_month(today->year, today->month);
    scene.today_cell = (uint8_t)(first_weekday + today->day - 1U);

    for (uint8_t day = 1U; day <= month_days; ++day) {
        uint8_t cell = (uint8_t)(first_weekday + day - 1U);
        uint8_t row = cell / 7U;
        uint8_t column = cell % 7U;
        calendar_date_t date = {today->year, today->month, day};
        lunar_date_t lunar;
        if (!lunar_from_solar(&date, &lunar)) return false;
        special_day_match_t special = special_day_match_on(&date);
        scene.special_cells[cell] = (uint8_t)special.importance;
        format_day(scene.solar_text[cell], day);
        format_lunar(scene.lunar_text[cell], &lunar);

        uint16_t x = (uint16_t)(GRID_X + column * CELL_W);
        uint16_t y = (uint16_t)(GRID_Y + row * CELL_H);
        bool major = special.importance == SPECIAL_MAJOR;
        epd_color_t solar_color = major ? EPD_COLOR_WHITE :
                                  ((column == 0U) ? EPD_COLOR_RED : EPD_COLOR_BLACK);
        epd_color_t lunar_color = major ? EPD_COLOR_WHITE : EPD_COLOR_BLACK;
        add_text(scene.solar_text[cell], (uint16_t)(x + 4U), (uint16_t)(y - 3U), 1U, solar_color);
        add_text(scene.lunar_text[cell], (uint16_t)(x + 31U), (uint16_t)(y + 17U), 1U, lunar_color);
    }
    uint16_t y_cord = 538U;
    special_day_match_t upcoming[UPCOMING_MAX];
    special_day_match_t special = special_day_match_on(today);
    if(special.importance){
        if(special.importance == SPECIAL_MAJOR){ 
            out = append_string(scene.today_special, "HÔM NAY LÀ NGÀY ");
            out = append_string(out, special.name);
            *out = '\0';
            add_text(scene.today_special, 14, y_cord, 1U,EPD_COLOR_RED);
        } 
        else{
            out = append_string(scene.today_special, "Hôm nay là ngày ");
            out = append_string(out, special.name);
            *out = '\0';
            add_text(scene.today_special, 14, y_cord, 1U,EPD_COLOR_BLACK);
        }
       y_cord += 24U;
    }
    uint8_t upcoming_count = special_days_upcoming(today, UPCOMING_DAYS, upcoming, UPCOMING_MAX);
    if (upcoming_count == 0U) {
        out = append_string(scene.upcoming[0], "Không có ngày đặc biệt trong ");
        out = append_u16(out, UPCOMING_DAYS, 1U);
        out = append_string(out, " ngày tới");
        *out = '\0';
        add_text(scene.upcoming[0], 14U, y_cord, 1U, EPD_COLOR_BLACK);
    } else {
        copy_bounded(scene.upcoming_label, sizeof(scene.upcoming_label), "NGÀY SẮP TỚI:");
        add_text(scene.upcoming_label, 14U, y_cord, 1U, EPD_COLOR_BLACK);
        y_cord += 24U;
        for (uint8_t i = 0U; i < upcoming_count; ++i) {
            format_upcoming(scene.upcoming[i], &upcoming[i]);
            add_text(scene.upcoming[i], 22U, (uint16_t)(y_cord + i * 24U), 1U,
                     (upcoming[i].importance == SPECIAL_MAJOR) ? EPD_COLOR_RED : EPD_COLOR_BLACK);
        }
    }
    return true;
}

bool calendar_render_prepare_low_battery(uint16_t millivolts) {
    scene.item_count = 0U;
    scene.digit_count = 0U;
    scene.mode = RENDER_SCENE_LOW_BATTERY;
    scene.column_valid = false;
    copy_bounded(scene.weekday, sizeof(scene.weekday), "PIN QUÁ YẾU");
    copy_bounded(scene.today_special, sizeof(scene.today_special),
                 "TẠM DỪNG LÀM MỚI MÀN HÌNH");
    format_battery_text(millivolts, true);
    add_text_center(scene.weekday, PORTRAIT_WIDTH / 2U, 210U, 2U, EPD_COLOR_RED);
    add_text_center(scene.today_special, PORTRAIT_WIDTH / 2U, 300U, 1U, EPD_COLOR_BLACK);
    scene.battery_item.y = 350U;
    scene.battery_item.x = (scene.battery_item.width < PORTRAIT_WIDTH) ?
                           (uint16_t)((PORTRAIT_WIDTH - scene.battery_item.width) / 2U) : 0U;
    return true;
}

bool calendar_render_prepare_ble_sync(void) {
    scene.item_count = 0U;
    scene.digit_count = 0U;
    scene.mode = RENDER_SCENE_BLE_SYNC;
    scene.column_valid = false;

    add_text_center("ĐỒNG BỘ GIỜ", PORTRAIT_WIDTH / 2U, 12U, 2U, EPD_COLOR_RED);
    add_text_center("QUÉT MÃ QR ĐỂ MỞ TRANG ĐỒNG BỘ",
                    PORTRAIT_WIDTH / 2U, 67U, 1U, EPD_COLOR_BLACK);
    add_text_center("1. QUÉT MÃ QR BẰNG CAMERA",
                    PORTRAIT_WIDTH / 2U, 377U, 1U, EPD_COLOR_BLACK);
    add_text_center("2. MỞ TRANG BẰNG CHROME",
                    PORTRAIT_WIDTH / 2U, 411U, 1U, EPD_COLOR_BLACK);
    add_text_center("3. NHẤN ĐỒNG BỘ THỜI GIAN",
                    PORTRAIT_WIDTH / 2U, 445U, 1U, EPD_COLOR_BLACK);
    add_text_center("4. CHỌN THIẾT BỊ VNCALENDAR",
                    PORTRAIT_WIDTH / 2U, 479U, 1U, EPD_COLOR_BLACK);
    add_text_center("DÙNG CHROME TRÊN ANDROID HOẶC PC",
                    PORTRAIT_WIDTH / 2U, 525U, 1U, EPD_COLOR_RED);

    char *out = append_string(scene.ble_timeout_text, "BLE TỰ TẮT SAU ");
    out = append_u16(out, BLE_WINDOW_SECONDS, 1U);
    out = append_string(out, " GIÂY");
    *out = '\0';
    add_text_center(scene.ble_timeout_text, PORTRAIT_WIDTH / 2U,
                    571U, 1U, EPD_COLOR_BLACK);
    add_text_center("THIẾT BỊ: VNCALENDAR", PORTRAIT_WIDTH / 2U,
                    605U, 1U, EPD_COLOR_BLACK);
    return true;
}

static void set_range(uint16_t first, uint16_t last, epd_color_t color) {
    if (first >= PORTRAIT_HEIGHT) return;
    if (last >= PORTRAIT_HEIGHT) last = PORTRAIT_HEIGHT - 1U;
    for (uint16_t y = first; y <= last; ++y) scene.column_pixels[y] = (uint8_t)color;
}

static void fill_rect_column(uint16_t x, uint16_t x0, uint16_t y0,
                             uint16_t x1, uint16_t y1, epd_color_t color) {
    if ((x >= x0) && (x <= x1)) set_range(y0, y1, color);
}

static void fill_dot_column(uint16_t x, uint16_t center_x, uint16_t center_y,
                            epd_color_t color) {
    uint16_t distance = (x > center_x) ? (uint16_t)(x - center_x) :
                                        (uint16_t)(center_x - x);
    if (distance > 3U) return;
    uint16_t half_height = (distance <= 1U) ? 3U : ((distance == 2U) ? 2U : 1U);
    set_range((uint16_t)(center_y - half_height),
              (uint16_t)(center_y + half_height), color);
}

static void outline_rect_column(uint16_t x, uint16_t x0, uint16_t y0,
                                uint16_t x1, uint16_t y1, uint8_t thickness,
                                epd_color_t color) {
    if ((x < x0) || (x > x1)) return;
    if ((x < x0 + thickness) || (x > x1 - thickness)) {
        set_range(y0, y1, color);
    } else {
        set_range(y0, (uint16_t)(y0 + thickness - 1U), color);
        set_range((uint16_t)(y1 - thickness + 1U), y1, color);
    }
}

static void draw_text_column(const text_item_t *item, uint16_t x) {
    uint16_t rendered_width = (uint16_t)(item->width * item->scale);
    if ((x < item->x) || (x >= (uint16_t)(item->x + rendered_width))) return;

    uint16_t local_x = (uint16_t)(x - item->x);
    const char *cursor = item->text;
    while (*cursor != '\0') {
        const font_vi_glyph_t *glyph = font_vi_find(font_vi_utf8_next(&cursor));
        uint16_t span = (uint16_t)(glyph->advance * item->scale);
        if (local_x < span) {
            uint8_t glyph_x = (uint8_t)(local_x / item->scale);
            if (glyph_x < glyph->width) {
                for (uint8_t glyph_y = 0U; glyph_y < FONT_VI_HEIGHT; ++glyph_y) {
                    if (!font_vi_pixel(glyph, glyph_x, glyph_y)) continue;
                    uint16_t output_y = (uint16_t)(item->y + glyph_y * item->scale);
                    for (uint8_t scale_y = 0U; scale_y < item->scale; ++scale_y) {
                        uint16_t y = (uint16_t)(output_y + scale_y);
                        if (y < PORTRAIT_HEIGHT) scene.column_pixels[y] = (uint8_t)item->color;
                    }
                }
            }
            return;
        }
        local_x = (uint16_t)(local_x - span);
    }
}

static void draw_digit_column(const digit_item_t *item, uint16_t x) {
    if ((x < item->x) || (x >= (uint16_t)(item->x + item->width))) return;

    uint16_t local_x = (uint16_t)(x - item->x);
    const char *cursor = item->text;
    while (*cursor != '\0') {
        const font_digit_glyph_t *glyph = font_digits_find(item->style, *cursor++);
        if (glyph == 0) continue;
        if (local_x < glyph->advance) {
            if (local_x < glyph->width) {
                for (uint8_t glyph_y = 0U; glyph_y < glyph->height; ++glyph_y) {
                    if (!font_digits_pixel(glyph, (uint8_t)local_x, glyph_y)) continue;
                    uint16_t y = (uint16_t)(item->y + glyph_y);
                    if (y < PORTRAIT_HEIGHT) scene.column_pixels[y] = (uint8_t)item->color;
                }
            }
            return;
        }
        local_x = (uint16_t)(local_x - glyph->advance);
    }
}

static void draw_ble_qr_column(uint16_t x) {
    const uint16_t data_x = (uint16_t)(BLE_QR_X +
        BLE_QR_QUIET_MODULES * BLE_QR_MODULE_PIXELS);
    const uint16_t data_y = (uint16_t)(BLE_QR_Y +
        BLE_QR_QUIET_MODULES * BLE_QR_MODULE_PIXELS);
    const uint16_t data_size = (uint16_t)(BLE_SYNC_QR_MODULES * BLE_QR_MODULE_PIXELS);

    if ((x < data_x) || (x >= (uint16_t)(data_x + data_size))) return;
    uint8_t module_x = (uint8_t)((x - data_x) / BLE_QR_MODULE_PIXELS);
    for (uint8_t module_y = 0U; module_y < BLE_SYNC_QR_MODULES; ++module_y) {
        if (!ble_sync_qr_module(module_x, module_y)) continue;
        uint16_t first = (uint16_t)(data_y + module_y * BLE_QR_MODULE_PIXELS);
        set_range(first, (uint16_t)(first + BLE_QR_MODULE_PIXELS - 1U),
                  EPD_COLOR_BLACK);
    }
}

static void prepare_column(uint16_t x) {
    for (uint16_t y = 0U; y < PORTRAIT_HEIGHT; ++y)
        scene.column_pixels[y] = (uint8_t)EPD_COLOR_WHITE;

    if (scene.mode == RENDER_SCENE_LOW_BATTERY) {
        outline_rect_column(x, 4U, 4U, 379U, 635U, 1U, EPD_COLOR_RED);
        for (uint8_t i = 0U; i < scene.item_count; ++i) draw_text_column(&scene.items[i], x);
        draw_text_column(&scene.battery_item, x);
        scene.cached_x = x;
        scene.column_valid = true;
        return;
    }

    if (scene.mode == RENDER_SCENE_BLE_SYNC) {
        outline_rect_column(x, 4U, 4U, 379U, 635U, 1U, EPD_COLOR_RED);
        draw_ble_qr_column(x);
        for (uint8_t i = 0U; i < scene.item_count; ++i) draw_text_column(&scene.items[i], x);
        scene.cached_x = x;
        scene.column_valid = true;
        return;
    }

    /* Weekday header: red Sunday, black Monday-Friday, red Saturday. */
    fill_rect_column(
        x,
        GRID_X,
        WEEKDAY_BAR_Y,
        (uint16_t)(GRID_X + CELL_W - 1U),
        (uint16_t)(WEEKDAY_BAR_Y + WEEKDAY_BAR_H - 1U),
        EPD_COLOR_RED
    );

    fill_rect_column(
        x,
        (uint16_t)(GRID_X + CELL_W),
        WEEKDAY_BAR_Y,
        (uint16_t)(GRID_X + 6U * CELL_W - 1U),
        (uint16_t)(WEEKDAY_BAR_Y + WEEKDAY_BAR_H - 1U),
        EPD_COLOR_BLACK
    );

    fill_rect_column(
        x,
        (uint16_t)(GRID_X + 6U * CELL_W),
        WEEKDAY_BAR_Y,
        (uint16_t)(GRID_X + 7U * CELL_W ),
        (uint16_t)(WEEKDAY_BAR_Y + WEEKDAY_BAR_H - 1U),
        EPD_COLOR_RED
    );

    /* Major holidays use a solid red cell interior. */
    for (uint8_t cell = 0U; cell < 42U; ++cell) {
        if (scene.special_cells[cell] != SPECIAL_MAJOR) continue;
        uint16_t cell_x = (uint16_t)(GRID_X + (cell % 7U) * CELL_W);
        uint16_t cell_y = (uint16_t)(GRID_Y + (cell / 7U) * CELL_H);
        fill_rect_column(x, (uint16_t)(cell_x + 1U), (uint16_t)(cell_y + 1U),
                         (uint16_t)(cell_x + CELL_W - 1U),
                         (uint16_t)(cell_y + CELL_H - 1U), EPD_COLOR_RED);
    }

    /* Structural black lines. */
    outline_rect_column(x, 4U, 4U, 379U, 635U, 1U, EPD_COLOR_RED);
    if (x == 192U) set_range(68U, 235U, EPD_COLOR_BLACK);
    if ((x >= 8U) && (x <= 376U)) {
        // set_range(248U, 249U, EPD_COLOR_BLACK);
        set_range(526U, 528U, EPD_COLOR_BLACK);
    }
    if ((x >= GRID_X) && (x <= GRID_X + 7U * CELL_W)) {
        for (uint8_t row = 0U; row <= 6U; ++row)
            scene.column_pixels[GRID_Y + row * CELL_H] = (uint8_t)EPD_COLOR_BLACK;
    }
    for (uint8_t column = 0U; column <= 7U; ++column) {
        if (x == (uint16_t)(GRID_X + column * CELL_W))
            set_range(GRID_Y, GRID_Y + 6U * CELL_H, EPD_COLOR_BLACK);
    }

    for (uint8_t i = 0U; i < scene.digit_count; ++i) draw_digit_column(&scene.digit_items[i], x);
    for (uint8_t i = 0U; i < scene.item_count; ++i) draw_text_column(&scene.items[i], x);

    /* Reserve the lower-right corner for the voltage readout. */
    if (scene.battery_valid) {
        fill_rect_column(x, (scene.battery_item.width < 370U) ? (uint16_t)(378U - scene.battery_item.width): 244U, 606U, 373U, 630U, EPD_COLOR_WHITE);
        draw_text_column(&scene.battery_item, x);
    }

    /* Today always receives the requested red outline. */
    uint16_t today_x = (uint16_t)(GRID_X + (scene.today_cell % 7U) * CELL_W);
    uint16_t today_y = (uint16_t)(GRID_Y + (scene.today_cell / 7U) * CELL_H);
    outline_rect_column(x, today_x, today_y, (uint16_t)(today_x + CELL_W),
                        (uint16_t)(today_y + CELL_H), 3U, EPD_COLOR_RED);

    /* Minor special days retain the compact red marker. */
    for (uint8_t cell = 0U; cell < 42U; ++cell) {
        if (scene.special_cells[cell] != SPECIAL_MINOR) continue;
        uint16_t marker_x = (uint16_t)(GRID_X + (cell % 7U) * CELL_W + 46U);
        uint16_t marker_y = (uint16_t)(GRID_Y + (cell / 7U) * CELL_H + 8U);
        fill_dot_column(x, marker_x, marker_y, EPD_COLOR_RED);
    }

    scene.cached_x = x;
    scene.column_valid = true;
}

epd_color_t calendar_render_pixel(uint16_t native_x, uint16_t native_y, void *context) {
    (void)context;
    if ((native_x >= EPD7IN5B_WIDTH) || (native_y >= EPD7IN5B_HEIGHT)) return EPD_COLOR_WHITE;

#if CALENDAR_ROTATION == 90
    uint16_t logical_x = (uint16_t)(PORTRAIT_WIDTH - 1U - native_y);
    uint16_t logical_y = native_x;
#else
    uint16_t logical_x = native_y;
    uint16_t logical_y = (uint16_t)(PORTRAIT_HEIGHT - 1U - native_x);
#endif

    if (!scene.column_valid || (scene.cached_x != logical_x)) prepare_column(logical_x);
    return (epd_color_t)scene.column_pixels[logical_y];
}
