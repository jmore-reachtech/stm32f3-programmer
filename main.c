#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include "main.h"
#include "serial.h"
#include "gpio.h"
#include "stm32.h"

static void reset_micro(pin_state s);
static int update_firmware(char *path);
static int start(void);
static void read_action(void);
static void write_action(void);
static void query_action(void);

struct {
	work_action task;
	work_state state;
	uint8_t reset;
	char *filename;
	uint32_t addr;
	serial_port_options sport;
} work = {
	.task = FLASH_NONE,
	.state = IDLE,
	.reset = 0x1,
	.filename = "main.bin",
	.addr = USER_DATA_OFFSET,
	.sport = {
		.device = TTY_DEV,
		.baud_rate = B57600,
	},
};

static void reset_micro(pin_state s)
{
	gpio_toggle_boot(s);
	sleep(1);
	gpio_toggle_reset(LOW);
	sleep(1);
	gpio_toggle_reset(HIGH);
	sleep(1);
}

static void sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	reset_micro(LOW);

	gpio_deinit();
	serial_deinit();
}

static int update_firmware(char *path)
{
	FILE *fp;
	ssize_t r;
	long size;
	uint8_t tmp[MAX_RW_SIZE];
	uint32_t addr = STM_FLASH_BASE;
	unsigned int i;

	fp = fopen(path, "rb");
	if(fp == NULL) {
		fprintf(stderr, "File '%s' not found!", path);
		return 1;
	}

	fseek (fp , 0 , SEEK_END);
	size = ftell (fp);
	rewind (fp);

	printf("%s: file size is %ld \n", __func__, size);

	r = fread (tmp,1,MAX_RW_SIZE,fp);
	while (r > 0) {
		if(r < MAX_RW_SIZE) {
			printf("\n%s: padding buffer, read %d \n", __func__, r);
			for(i = r; i < MAX_RW_SIZE; i++) {
				tmp[i] = 0xFF;
			}
		}
		printf("\n%s: writing %d bytes to flash \n", __func__, MAX_RW_SIZE);
		stm_write_mem(addr,tmp,MAX_RW_SIZE);
		addr += r;

		r = fread (tmp,1,MAX_RW_SIZE,fp);
	}

	fclose (fp);
	
	return 0;
}

static void display_help(char *prog_name)
{
	fprintf(stdout, "Usage: %s [options]\n", prog_name);
	fprintf(stdout, "  Program external micro\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "  -b baud_rate          Set baud rate (default:%s)\n", baud_key_to_str(work.sport.baud_rate));
    fprintf(stdout, "  -t tty_device         Set serial dev (default:%s)\n", work.sport.device);
    fprintf(stdout, "  -w filename           Write flash from file (default:%s)\n", work.filename);
    fprintf(stdout, "  -r filename           Read flash to file (default:%s)\n", work.filename);
    fprintf(stdout, "  -s                    Skip micro reset (default:%s)\n", work.reset ? "No" : "Yes");
    fprintf(stdout, "  -q                    Query micro version(default:0x%08X)\n", work.addr);
    fprintf(stdout, "  -h                    Display this help and exit\n");
    fprintf(stdout, "\n");
}

static int parse_options(int argc, char *argv[]) 
{
	int c;
	
	while ((c = getopt(argc, argv, "hw:r:b:t:sq")) != -1) {
		switch(c) {
			case 'h':
				if(work.task != FLASH_NONE) {
					fprintf(stderr,"Multiple actions not supported! \n");
					return 1;
				}
				work.task = FLASH_HELP;
				;break;
			case 'w':
				if(work.task != FLASH_NONE) {
					fprintf(stderr,"Multiple actions not supported! \n");
					return 1;
				}
				work.filename = optarg;
				work.task = FLASH_WRITE;
				break;
			case 'r':
				if(work.task != FLASH_NONE) {
					fprintf(stderr,"Multiple actions not supported! \n");
					return 1;
				}
				work.filename = optarg;
				work.task = FLASH_READ;
				break;
			case 'b':
				work.sport.baud_rate = baud_str_to_key(optarg);
				break;
			case 't':
				work.sport.device = strdup(optarg);
				break;
			case 's':
				work.reset = 0x0;
				break;
			case 'q':
				if(work.task != FLASH_NONE) {
					fprintf(stderr,"Multiple actions not supported! \n");
					return 1;
				}
				work.task = FLASH_QUERY;
				break;
		}
	}
	
	return 0;
}

static void write_action(void)
{
	if(work.reset) {
		if(gpio_init() != 0) {
			work.state = FAILED;
			goto err;
		}
		reset_micro(HIGH);
	}
	
	serial_init();
	work.state = INITED;
	if(stm_init_seq() != 0) {
		work.state = FAILED;
		goto deinit;
	}
	if(stm_erase_mem() != 0) {
		work.state = FAILED;
		goto deinit;
	}
	update_firmware(work.filename);
	work.state = SUCCESS;
	
deinit:
if(work.reset) {
	reset_micro(LOW);
	gpio_deinit();
}
serial_deinit();

err:
	return;
}

static void query_action(void)
{
	uint8_t data[4] = {0};
	uint32_t addr = 0x0;
	
	if(work.reset) {
		if(gpio_init() != 0) {
			work.state = FAILED;
			goto err;
		}
		reset_micro(HIGH);
	}
	
	serial_init();
	work.state = INITED;
	if(stm_init_seq() != 0) {
		work.state = FAILED;
		goto deinit;
	}
	if(stm_read_mem(work.addr, data, 4) != 0) {
		work.state = FAILED;
		goto deinit;
	}

	addr = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0] << 0;
	if(addr == MICRO_VERSION) {
		printf("%s: version correct 0x%08X \n", __func__, addr);
	} else {
		printf("%s: version mismatch extected 0x%08X, got 0x%08X \n", 
			__func__, MICRO_VERSION, addr);
	}
	
	work.state = SUCCESS;
	
deinit:
if(work.reset) {
	reset_micro(LOW);
	gpio_deinit();
}
serial_deinit();

err:
	return;
}

static void read_action(void)
{
	fprintf(stdout, "Flash read not implemented \n");
	work.state = SUCCESS;
}

static int start(void)
{
	work.state = START;
	
	switch(work.task) {
		case FLASH_WRITE:
			write_action();
			break;
		case FLASH_READ:
			read_action();
			break;
		case FLASH_QUERY:
			query_action();
			break;
		default:
			break;
	}
	
	return work.state;
}

int main(int argc, char **argv)
{
	struct sigaction sig_act;
	int ret = 1;

	sig_act.sa_sigaction = sig_handler;
	sig_act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &sig_act, NULL) < 0) {
		perror ("sigaction");
		goto close;
	}
	if (sigaction(SIGINT, &sig_act, NULL) < 0) {
		perror ("sigaction");
		goto close;
	}
	
	if (parse_options(argc, argv) != 0) {
		goto close;
	}
	
	if(work.task == FLASH_HELP || work.task == FLASH_NONE) {
		display_help(argv[0]);
		goto close;
	}

	ret = start();

close:

	return ret;
}
