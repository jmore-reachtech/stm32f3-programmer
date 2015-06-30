#ifndef _SERIAL_H
#define _SERIAL_H

#define TTY_DEV "/dev/ttymxc4"

void serial_init(void);
void serial_deinit(void);
int serial_read(void *buf, size_t nbyte);
int serial_write(void *buf, size_t nbyte);

#endif // _SERIAL_H
