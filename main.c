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

#ifdef DEBUG
#define LOG(format, ...) printf(format "\n" , ##__VA_ARGS__);
#else
#define LOG(format, ...) 
#endif

static void reset_micro(pin_state s);
static int update_firmware(char *path);
static int start(void);
static void read_action(void);
static void write_action(void);
static void query_action(void);
static void go_action(void);

struct {
	work_action task;
	work_state state;
	uint8_t reset;
	char *filename;
	uint32_t addr;
	version_check ver_check;
	serial_port_options sport;
} work = {
	.task = FLASH_NONE,
	.state = IDLE,
	.reset = 1,
	.filename = "main.bin",
	.addr = USER_DATA_OFFSET,
	.ver_check = UNCHECKED,
	.sport = {
		.device = TTY_DEV,
		.baud_rate = B57600,
	},
};

static void reset_micro(pin_state s)
{
	LOG("%s: ", __func__);

	gpio_toggle_boot(s);
	sleep(1);
	gpio_toggle_reset(LOW);
	sleep(1);
	gpio_toggle_reset(HIGH);
	sleep(1);
}

static void sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	if(work.reset) {
		reset_micro(LOW);
		gpio_deinit();
	}

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
		LOG("File '%s' not found!", path);
		return 1;
	}

	fseek (fp , 0 , SEEK_END);
	size = ftell (fp);
	rewind (fp);

	LOG("%s: file size is %ld", __func__, size);

	r = fread (tmp,1,MAX_RW_SIZE,fp);
	while (r > 0) {
		if(r < MAX_RW_SIZE) {
			LOG("\n%s: padding buffer, read %d", __func__, r);
			for(i = r; i < MAX_RW_SIZE; i++) {
				tmp[i] = 0xFF;
			}
		}
		LOG("\n%s: writing %d bytes to flash", __func__, MAX_RW_SIZE);
		stm_write_mem(addr,tmp,MAX_RW_SIZE);
		addr += r;

		r = fread (tmp,1,MAX_RW_SIZE,fp);
	}

	fclose (fp);
	
	return 0;
}

static void display_version(void)
{
   fprintf(stdout, "\tApp version: %d.%d.%d \n", VERSION_MAJOR(APP_VERSION),
        VERSION_MINOR(APP_VERSION), VERSION_PATCH(APP_VERSION)); 
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
    fprintf(stdout, "  -v                    Display version and exit\n");
    fprintf(stdout, "  -h                    Display this help and exit\n");
    fprintf(stdout, "\n");
}

static int parse_options(int argc, char *argv[]) 
{
	int c;
	
	while ((c = getopt(argc, argv, "vhw:r:b:t:sq")) != -1) {
		switch(c) {
			case 'h':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
					return 1;
				}
				work.task = FLASH_HELP;
				;break;
            case 'v':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
					return 1;
				}
				work.task = FLASH_VERSION;
                break;
			case 'w':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
					return 1;
				}
				work.filename = optarg;
				work.task = FLASH_WRITE;
				break;
			case 'r':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
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
				work.reset = 0;
				break;
			case 'q':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
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
	if(stm_erase_mem() != 0) {
		work.state = FAILED;
		goto err;
	}
	update_firmware(work.filename);
	work.state = SUCCESS;

err:
	return;
}

static void query_action(void)
{
	uint8_t data[4] = {0};
	uint32_t addr = 0x0;

	if(stm_read_mem(work.addr, data, 4) != 0) {
		work.state = FAILED;
		goto err;
	}

	addr = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0] << 0;
    
    fprintf(stdout, "\tMicro version: %d.%d.%d \n", VERSION_MAJOR(addr),
        VERSION_MINOR(addr), VERSION_PATCH(addr));

	if(addr == APP_VERSION) {
		work.ver_check = MATCH;
	} else {
		work.ver_check = MISMATCH;
	}
	
	work.state = SUCCESS;
err:
	return;
}

static void go_action(void)
{
	if(stm_go(STM_FLASH_BASE) != 0) {
		work.state = FAILED;
		goto err;
	}

	sleep(5);
	work.state = SUCCESS;

err:
	return;
}


static void read_action(void)
{
	LOG("Flash read not implemented");
	work.state = SUCCESS;
}

static int start(void)
{
	work.state = START;

	if(work.reset) {
		LOG("performing reset!");
		if(gpio_init() != 0) {
			LOG("gpio init failed!");
			work.state = FAILED;
			goto err;
		}
		reset_micro(HIGH);
	} else {
		sleep(1);
	}
	serial_init();
	if(stm_init_seq() != 0) {
		work.state = FAILED;
		goto deinit;
	}
	work.state = INITED;
	
	switch(work.task) {
		case FLASH_WRITE:
			write_action();
			break;
		case FLASH_READ:
			read_action();
			break;
		case FLASH_QUERY:
			query_action();
			if(work.ver_check != MATCH) {
				LOG("Need to update micro!");
			}
			break;
		default:
			break;
	}
	go_action();

deinit:
	if(work.reset) {
		reset_micro(LOW);
		gpio_deinit();
	}
	serial_deinit();
err:
	return work.state;
}

//static void write_file(uint8_t *data, int size)
//{
	//FILE *fp;
	//ssize_t r;
	
	//fp = fopen("flash.bin", "w+b");
	
	//r = fwrite(data, sizeof(uint8_t), size, fp);
	
	//printf("%s: wrote %d bytes \n", __func__, r);
//}

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
    
	if(work.task == FLASH_VERSION) {
        display_version();
        goto close;
    }

	ret = start();

close:

	return ret;
}
