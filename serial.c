#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

#include "serial.h"

static int fd = 0;

static void serial_tio_init(struct termios *t);

static void serial_tio_init(struct termios *t)
{
	cfmakeraw(t);
    t->c_cflag &= ~(CSIZE | CRTSCTS);
    t->c_cflag &= ~(CSIZE | CRTSCTS);
	t->c_iflag &= ~(IXON | IXOFF | IXANY | IGNPAR);
	t->c_lflag &= ~(ECHOK | ECHOCTL | ECHOKE);
	t->c_oflag &= ~(OPOST | ONLCR);
    t->c_cflag |= INPCK | PARENB | CS8 | CLOCAL | CREAD;

    t->c_cc[VMIN] = 0;
    t->c_cc[VTIME] = 5;
}

void serial_init(void)
{
	struct termios tio;
	
	fd = open(TTY_DEV, O_RDWR | O_NOCTTY);
	if (fd == -1) {
		/* Could not open the port. */
		printf("open_port: Unable to open %s - \n", TTY_DEV);
		return;
	}
    
    serial_tio_init(&tio);
    tcflush(fd, TCIFLUSH);
    cfsetospeed(&tio, B57600);
	cfsetispeed(&tio, B57600);
	tcsetattr(fd, TCSANOW, &tio);
}

void serial_deinit(void)
{
	if(fd) {
		close(fd);
	}
}

int serial_read(void *buf, size_t nbyte)
{
	ssize_t r;
	uint8_t *pos = (uint8_t *)buf;

	printf("%s: reading %d bytes \n", __func__, nbyte);
	while (nbyte) {
		r = read(fd, pos, nbyte);
		if (r == 0)
			return 1;
		if (r < 0)
			return 1;

		nbyte -= r;
		pos += r;
	}
	return 0;
}

int serial_write(void *buf, size_t nbyte)
{
	ssize_t r;
	
	printf("%s: writing %d bytes \n", __func__, nbyte);
	r = write(fd, buf, nbyte);
	
	return r;
}
