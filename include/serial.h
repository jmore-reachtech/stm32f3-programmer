#ifndef _SERIAL_H
#define _SERIAL_H

#define TTY_DEV "/dev/ttymxc4"
#define SERIAL_BUF_MAX 512

struct serial_port_options {
    int fd;
	const char *device;
	uint32_t baud_rate;
};

int serial_init(struct serial_port_options *opts);
void serial_deinit(struct serial_port_options *opts);
int serial_read(struct serial_port_options *opts, void *buf, size_t nbyte);
int serial_write(struct serial_port_options *opts, void *buf, size_t nbyte);
uint32_t serial_baud_str_to_key(const char *baud_str);
const char *serial_baud_key_to_str(uint32_t baud_key);

#endif // _SERIAL_H
