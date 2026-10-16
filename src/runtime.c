/*
 * Minimal freestanding C runtime functions required when GCC lowers an
 * aggregate structure copy or initialization to a library call. The firmware
 * deliberately links with -nostdlib, so these cannot come from newlib.
 */

#include <stddef.h>

void *memcpy(void *destination, const void *source, size_t length) {
    unsigned char *dst = (unsigned char *)destination;
    const unsigned char *src = (const unsigned char *)source;

    while (length-- != 0U) {
        *dst++ = *src++;
    }
    return destination;
}

void *memset(void *destination, int value, size_t length) {
    unsigned char *dst = (unsigned char *)destination;

    while (length-- != 0U) {
        *dst++ = (unsigned char)value;
    }
    return destination;
}
