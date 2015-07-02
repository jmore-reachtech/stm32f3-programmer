#ifndef _SERIAL_H
#define _SERIAL_H

#define TTY_DEV "/dev/ttymxc4"

typedef struct serial_port_options {
	const char *device;
	uint32_t baud_rate;
} serial_port_options;

void serial_init(void);
void serial_deinit(void);
int serial_read(void *buf, size_t nbyte);
int serial_write(void *buf, size_t nbyte);
uint32_t baud_str_to_key(const char *baud_str);
const char *baud_key_to_str(uint32_t baud_key);

#endif // _SERIAL_H
