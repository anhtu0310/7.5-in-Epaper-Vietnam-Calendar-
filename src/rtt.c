#include "rtt.h"

#include <stddef.h>

#define RTT_BUFFER_SIZE 1024U

typedef struct {
    const char *name;
    char *buffer;
    uint32_t size;
    volatile uint32_t write_offset;
    volatile uint32_t read_offset;
    uint32_t flags;
} rtt_buffer_up_t;

typedef struct {
    const char *name;
    char *buffer;
    uint32_t size;
    volatile uint32_t write_offset;
    volatile uint32_t read_offset;
    uint32_t flags;
} rtt_buffer_down_t;

typedef struct {
    char id[16];
    int32_t max_up_buffers;
    int32_t max_down_buffers;
    rtt_buffer_up_t up[1];
    rtt_buffer_down_t down[1];
} rtt_control_block_t;

static char rtt_up_buffer[RTT_BUFFER_SIZE];
static const char rtt_terminal_name[] = "Terminal";

__attribute__((section(".rtt"), used, aligned(4)))
volatile rtt_control_block_t _SEGGER_RTT;

void rtt_init(void) {
    static const char signature[16] = {'S', 'E', 'G', 'G', 'E', 'R', ' ', 'R', 'T', 'T', 0, 0, 0, 0, 0, 0};

    for (uint32_t i = 0U; i < 16U; ++i) _SEGGER_RTT.id[i] = signature[i];
    _SEGGER_RTT.max_up_buffers = 1;
    _SEGGER_RTT.max_down_buffers = 0;
    _SEGGER_RTT.up[0].name = rtt_terminal_name;
    _SEGGER_RTT.up[0].buffer = rtt_up_buffer;
    _SEGGER_RTT.up[0].size = RTT_BUFFER_SIZE;
    _SEGGER_RTT.up[0].write_offset = 0U;
    _SEGGER_RTT.up[0].read_offset = 0U;
    _SEGGER_RTT.up[0].flags = 0U;
}

static void rtt_putc(char value) {
    rtt_buffer_up_t *up = (rtt_buffer_up_t *)&_SEGGER_RTT.up[0];
    uint32_t next = up->write_offset + 1U;
    if (next == up->size) next = 0U;

    /* Non-blocking: discard when no debugger is consuming the buffer. */
    if (next == up->read_offset) return;
    up->buffer[up->write_offset] = value;
    up->write_offset = next;
}

void rtt_write(const char *text) {
    if (text == NULL) return;
    while (*text != '\0') rtt_putc(*text++);
}

void rtt_write_u32(uint32_t value) {
    char digits[10];
    uint32_t count = 0U;

    if (value == 0U) {
        rtt_putc('0');
        return;
    }
    while (value != 0U) {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    while (count != 0U) rtt_putc(digits[--count]);
}
