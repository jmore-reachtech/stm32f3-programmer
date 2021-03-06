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

/* Uncomment for full debugging */
//#define DEBUG
#ifdef DEBUG
#define LOG(format, ...) printf(format "\n" , ##__VA_ARGS__);
#else
#define LOG(format, ...)
#endif

const struct {
    uint32_t    key;
    uint32_t    speed;
} BaudTable[] = {
    { B0      , 0       },
    { B50     , 50      },
    { B75     , 75      },
    { B110    , 110     },
    { B134    , 134     },
    { B150    , 150     },
    { B200    , 200     },
    { B300    , 300     },
    { B600    , 600     },
    { B1200   , 1200    },
    { B1800   , 1800    },
    { B2400   , 2400    },
    { B4800   , 4800    },
    { B9600   , 9600    },
    { B19200  , 19200   },
    { B38400  , 38400   },
    { B57600  , 57600   },
    { B115200 , 115200  },
    { B230400 , 230400  },
    { B460800 , 460800  },
    { B500000 , 500000  },
    { B576000 , 576000  },
    { B921600 , 921600  },
    { B1000000, 1000000 },
    { B1152000, 1152000 },
    { B1500000, 1500000 },
    { B2000000, 2000000 },
    { B2500000, 2500000 },
    { B3000000, 3000000 },
    { B3500000, 3500000 },
    { B4000000, 4000000 },
};

#define NELEM(a) (sizeof(a) / sizeof((a)[0]))

static void serial_tio_init(struct termios *t);

/* 
    Setup the termios struct 
*/
static void serial_tio_init(struct termios *t)
{
    /* We need to be in raw mode */
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

/* 
    Init the serial port 
*/
int serial_init(struct serial_port_options *opts)
{
	struct termios tio;
	
    /* Open the serial port */
	opts->fd = open(opts->device, O_RDWR | O_NOCTTY);
	if (opts->fd == -1) {
		/* Could not open the port. */
		LOG("open_port: Unable to open %s", opts->device);
		return opts->fd;
	}
    
    /* We have a file descriptor so set the serial options and baud rate */
    serial_tio_init(&tio);
    tcflush(opts->fd, TCIFLUSH);
    cfsetospeed(&tio, opts->baud_rate);
	cfsetispeed(&tio, opts->baud_rate);
	tcsetattr(opts->fd, TCSANOW, &tio);

    return opts->fd;
}

/* 
    This cleans up the serial port. We might want to save the old termios and 
    reset here 
*/
void serial_deinit(struct serial_port_options *opts)
{
	if(opts->fd) {
		close(opts->fd);
	}

    opts->fd = 0;
}

/* 
    Sometimes not all the data is available. This loops until we get the number 
    of bytes we expect. The number of bytes is based on the STM32 bootloader 
    protocol. 
*/
static int serial_read_all(int sfd, void *buf, size_t nbyte)
{
   	ssize_t r;
	uint8_t *pos = (uint8_t *)buf;
    int b_read = nbyte;

	LOG("%s: reading %d bytes", __func__, nbyte);
	while (b_read) {
		r = read(sfd, pos, b_read);
        /* incomplete read */
		if (r <= 0)
			return b_read;

		b_read -= r;
		pos += r;
	}
    
    /* we got here so b_read == nbyte, all good*/
	return nbyte; 
}

/* 
    Reads data from the serial port. If nbyte is 0 this is a regular read. If 
    we are talking to the STM32 bootloader we expect a certain number of bytes on
    each read. In that case call local serial_read_all() 
*/
int serial_read(struct serial_port_options *opts, void *buf, size_t nbyte)
{
    int b_read;

    if(nbyte > 0) {
        b_read = serial_read_all(opts->fd, buf, nbyte);
    } else {
        b_read = read(opts->fd, buf, SERIAL_BUF_MAX);
    }

    return b_read;
}

/* 
    Write data to the serial port. 
*/
int serial_write(struct serial_port_options *opts, void *buf, size_t nbyte)
{
	ssize_t r;
	
	LOG("%s: writing %d bytes", __func__, nbyte);
	r = write(opts->fd, buf, nbyte);
	
	return r;
}

/* 
    Helper function to convert baud rate to readable string 
*/
const char *serial_baud_key_to_str(uint32_t baud_key)
{
    static char buffer[32] = { 0 };
    int x;

    for (x = 0; x < NELEM(BaudTable); x++)
    {
        if (BaudTable[x].key == baud_key)
        {
            break;
        }
    }

    if (x >= NELEM(BaudTable))
    {
        sprintf(buffer, "{Unknown<%06oo>}", baud_key);
    }
    else
    {
        uint32_t speed = BaudTable[x].speed;

        sprintf(buffer, "%ubps", speed);
    }

    return buffer;
}

/* 
    Helper function to convert readable baud string to baud rate 
*/
uint32_t serial_baud_str_to_key(const char *baud_str)
{
    uint32_t baud_key = __MAX_BAUD;
    char *baud_buf = NULL;
    char buf[32];
    char *bp;
    int x;

    baud_buf = strdup(baud_str);
    if ((bp = strstr(baud_buf, "bps")) != NULL)
    {
        *bp = '\0';
    }

    for (x = 0; x < NELEM(BaudTable); x++)
    {
        sprintf(buf, "%u", BaudTable[x].speed);
        if (strcasecmp(buf, baud_buf) == 0)
        {
            baud_key = BaudTable[x].key;
            break;
        }
    }

    if (x >= NELEM(BaudTable))
    {
        LOG("Warning: Unknown baud rate '%s'. Defaulting to %s", baud_str, serial_baud_key_to_str(baud_key));
    }

    if (baud_buf != NULL)
    {
        free(baud_buf);
        baud_buf = NULL;
    }

    return baud_key;
}
