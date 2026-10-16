#ifndef RTT_H
#define RTT_H

#include <stdint.h>

void rtt_init(void);
void rtt_write(const char *text);
void rtt_write_u32(uint32_t value);

#endif
