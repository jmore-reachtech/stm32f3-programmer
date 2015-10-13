#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#include "common_p.h"
#include "serial.h"
#include "gpio.h"
#include "stm32.h"

/* Uncomment for full debugging */
//#define DEBUG
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
static void run_task(void);

/* 
    Structure to hold options and state 
*/
struct {
	work_action task;
	task_state_t task_state;
	stm32_state_t micro_state;
	uint8_t reset;
	char filename[128];
	uint32_t addr;
	version_check ver_check;
	struct serial_port_options sport;
} work = {
	.task = FLASH_NONE,
	.task_state = TASK_IDLE,
	.micro_state = STM32_IDLE,
	.reset = 1,
	.filename = "/home/root/main.bin",
	.addr = USER_DATA_OFFSET,
	.ver_check = UNCHECKED,
	.sport = {
        .fd = 0,
		.device = TTY_DEV,
		.baud_rate = B57600,
	},
};

/* 
    Reset the STM32. To put the STM32 in reset pull the boot pin high, 
    and toggle the reset pin. This will bring the STM32 up in bootloader mode.
    To return the STM32 to normal, pull the boot pin low and toggle the reset pin.
*/
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

/* 
    Set up the STM32 in bootloader mode. Here we init the serial port, 
    GPIO port and send the STM32 an init byte. 
*/
static void micro_init(void)
{
   	if(work.reset) {
		LOG("performing reset!");
		if(gpio_init() != 0) {
			LOG("gpio init failed!");
			work.micro_state = STM32_FAILED;
		}
		reset_micro(HIGH);
	} else {
		sleep(1);
	}
	serial_init(&(work).sport);
	if(stm_init_seq(&(work).sport) != 0) {
		work.micro_state = STM32_FAILED;
	}

	work.micro_state = STM32_READY;
}

/* 
    Reset the STM32 to normal running state, reset the GPIO and serial 
    port.
*/
static void micro_deinit(void)
{
   if(work.reset) {
		reset_micro(LOW);
		gpio_deinit();
	}
	serial_deinit(&(work).sport);
}

/* 
    Our signal handler. We listen for SIGTERM and SIGINT (Ctrl-C). Here 
    we reset the STM32 and close up the GPIO and serial port 
*/
static void sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	if(work.reset) {
		reset_micro(LOW);
		gpio_deinit();
	}

	serial_deinit(&(work).sport);
}

/* 
    Update the firmware on the STM32. This reads a file from the filesystem 
    and writes it to the STM32. The STM32 accepts 256 bytes for each write so
    we divide the file size by 256 to give us an ops count and use this to 
    notify the client of progress.
*/
static int update_firmware(char *path)
{
	FILE *fp;
	ssize_t r;
	long size;
	uint8_t tmp[MAX_RW_SIZE];
	uint32_t addr = STM_FLASH_BASE;
	unsigned int i, num_ops;
    int ret;

	fp = fopen(path, "rb");
	if(fp == NULL) {
		LOG("File '%s' not found!", path);
		return 1;
	}

	fseek (fp , 0 , SEEK_END);
	size = ftell (fp);
	rewind (fp);

    /* How many 256 byte chunks we have */
    num_ops = size / MAX_RW_SIZE;

	LOG("%s: file size is %ld; ops = %d, fw = %s\n", __func__,
            size, num_ops, path);

	r = fread (tmp,1,MAX_RW_SIZE,fp);
	while (r > 0) {
        /* We need to pad reads that are smaller than 256 bytes */
		if(r < MAX_RW_SIZE) {
			LOG("\n%s: padding buffer, read %d", __func__, r);
			for(i = r; i < MAX_RW_SIZE; i++) {
				tmp[i] = 0xFF;
			}
		}
		LOG("\n%s: writing %d bytes to flash", __func__, MAX_RW_SIZE);
		ret = stm_write_mem(&(work).sport, addr,tmp,MAX_RW_SIZE);
		addr += r;

        /* Write progress to stdout */
        fprintf(stdout,"%d\n", num_ops--);

		r = fread (tmp,1,MAX_RW_SIZE,fp);
	}

	fclose (fp);
	
	return 0;
}

/* 
    We can compile in an app version if need be. This is called by the 
    -v command line option 
*/
static void display_version(void)
{
   fprintf(stdout, "%d.%d.%d \n", VERSION_MAJOR(APP_VERSION),
        VERSION_MINOR(APP_VERSION), VERSION_PATCH(APP_VERSION)); 
}

/* 
    Show command line options 
*/
static void display_help(char *prog_name)
{
	fprintf(stdout, "Usage: %s [options]\n", prog_name);
	fprintf(stdout, "  Program external micro\n");
    fprintf(stdout, "\n");
    fprintf(stdout, "Options:\n");
    fprintf(stdout, "  -b baud_rate          Set baud rate (default:%s)\n", 
        serial_baud_key_to_str(work.sport.baud_rate));
    fprintf(stdout, "  -t tty_device         Set serial dev (default:%s)\n", 
        work.sport.device);
    fprintf(stdout, "  -w filename           Write flash from file (default:%s)\n", 
        work.filename);
    fprintf(stdout, "  -r filename           Read flash to file (default:%s)\n", 
        work.filename);
    fprintf(stdout, "  -s                    Skip micro reset (default:%s)\n", 
        work.reset ? "No" : "Yes");
    fprintf(stdout, "  -q                    Query micro version(default:0x%08X)\n", 
        work.addr);
    fprintf(stdout, "  -i                    Run in interactive mode\n");
    fprintf(stdout, "  -v                    Display version and exit\n");
    fprintf(stdout, "  -h                    Display this help and exit\n");
    fprintf(stdout, "\n");
}

/*
    Parse command line args. This sets up the task.
*/
static int parse_options(int argc, char *argv[]) 
{
	int c;
	
	while ((c = getopt(argc, argv, "ivhw:r:b:t:sq")) != -1) {
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
            case 'i':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
					return 1;
				}
				work.task = FLASH_INTERACTIVE;
                break;
			case 'w':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
					return 1;
				}
                strncpy(work.filename, optarg, sizeof(work.filename));
				work.task = FLASH_WRITE;
				break;
			case 'r':
				if(work.task != FLASH_NONE) {
					LOG("Multiple actions not supported!");
					return 1;
				}
                strncpy(work.filename, optarg, sizeof(work.filename));
				work.task = FLASH_READ;
				break;
			case 'b':
				work.sport.baud_rate = serial_baud_str_to_key(optarg);
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

/* 
    Write task, we are going to program the STM32 
*/
static void write_action(void)
{
	if(stm_erase_mem(&(work).sport) != 0) {
		work.micro_state = STM32_FAILED;
		goto err;
	}
	update_firmware(work.filename);
	work.task_state = TASK_SUCCESS;

err:
	return;
}

/* 
    Query task, read the STM32 application version. The version is stored 
    at a pre-determined address, USER_DATA_OFFSET.
*/
static void query_action(void)
{
	uint8_t data[4] = {0};
	uint32_t addr = 0x0;

	if(stm_read_mem(&(work).sport, work.addr, data, 4) != 0) {
		work.micro_state = STM32_FAILED;
		goto err;
	}

	addr = data[3] << 24 | data[2] << 16 | data[1] << 8 | data[0] << 0;

    fprintf(stdout, "%d.%d.%d \n", VERSION_MAJOR(addr),
        VERSION_MINOR(addr), VERSION_PATCH(addr));

	if(addr == APP_VERSION) {
		work.ver_check = MATCH;
	} else {
		work.ver_check = MISMATCH;
	}
	
	work.task_state = TASK_SUCCESS;
err:
	return;
}

/*
    Go task, just to flash base and start executing.
*/
static void go_action(void)
{
	if(stm_go(&(work).sport, STM_FLASH_BASE) != 0) {
		work.micro_state = STM32_FAILED;
		goto err;
	}

	sleep(5);
	work.task_state = TASK_SUCCESS;

err:
	return;
}

/*
    In interactive mode read a command from stdin.
*/
static void read_command(char *buf, int size)
{
    int len = 0;

    fprintf(stdout,"-> ");
    fgets(buf, size, stdin);
    len = strlen(buf);
    if(len > 0 && buf[len-1] == '\n') {
        buf[len-1] = '\0';
    }
}

/*
    Interactive mode command parser.
*/
static int cmd_parser(char *cmd)
{
    if (strcmp(cmd,"exit") == 0) {
        return CMD_EXIT;
    }
    if (strcmp(cmd,"help") == 0) {
        return CMD_HELP;
    }
    if (strcmp(cmd,"micro-ver") == 0) {
        return CMD_MICRO_VER;
    }
    if (strcmp(cmd,"app-ver") == 0) {
        return CMD_APP_VER;
    }
    if (strcmp(cmd,"update") == 0) {
        return CMD_UPDATE;
    }
    if (strcmp(cmd,"firmware") == 0) {
        return CMD_FIRMWARE;
    }
    if (strcmp(cmd,"status") == 0) {
        return CMD_STATUS;
    }
    
    return CMD_UNKNOWN;
}

/*
    Interactive mode, display the firmware file that will be used 
    for the update task.
*/
static void interactive_display_status()
{
    fprintf(stdout, "firmware: %s\n", work.filename);
}

/*
    Interactive mode, when we get a firmware command capture 
    the new firmware path. Users can use this to change what firmware
    is used for and update.
*/
static void interactive_read_firmware()
{
    char buf[32] = {0};
    int len = 0;

    fprintf(stdout,"-> firmware ");
    read_command(buf, sizeof(buf));
    
    len = strlen(buf);
    if(len > 0) {
        memset(&(work).filename, 0, sizeof(work.filename));
        strncpy(work.filename, buf, len);
    }
    
}

/*
    Interacive mode, display what commands we support.
*/
static void interactive_display_help()
{
    fprintf(stdout, "help \n");
    fprintf(stdout, "micro-ver \n");
    fprintf(stdout, "app-ver \n");
    fprintf(stdout, "status \n");
    fprintf(stdout, "update \n");
    fprintf(stdout, "firmware \n");
    fprintf(stdout, "exit \n");
}

/*
    Interactive mode, this is the main interactive task loop. Here 
    we accept commands from the user and complete requested actions.
*/
static void interactive_action(void)
{
    char cmd[32] = {0};

    micro_init();

    read_command(cmd, sizeof(cmd));

    while(1) {
        switch (cmd_parser(cmd)) {
            case CMD_EXIT:
                goto done;
                break;
            case CMD_HELP:
                interactive_display_help(); 
                break;
            case CMD_MICRO_VER:
                work.task = FLASH_QUERY;
                run_task();
                break;
            case CMD_STATUS:
                interactive_display_status();
                break;
            case CMD_FIRMWARE:
                interactive_read_firmware();
                break;
            case CMD_APP_VER:
                display_version();
                break;
            case CMD_UPDATE:
                work.task = FLASH_WRITE;
                run_task();
                break;
            default:
                fprintf(stdout, "Cmd  '%s' unkown\n", cmd); 
        }
        read_command(cmd, sizeof(cmd));
    }
done:
    micro_deinit();

    return;
}

/* 
    Read task, we can read the firmware from the STM32 and save to a file.
    Not implemented.
*/
static void read_action(void)
{
	LOG("Flash read not implemented");
	work.task_state = TASK_SUCCESS;
}

/*
    After all the IO is setup we run the task here.
*/
static void run_task(void)
{
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
}

/*
    This starts the process to run the task as parsed from the commannd line.
    Here we setup the STM32 and configure the GPIO and serial port.
*/
static int start(void)
{
	work.task_state = TASK_START;

    micro_init();
    if(work.task_state == TASK_FAILED) {
        goto deinit;
    }

    run_task();

	go_action();
    
    work.task_state = TASK_SUCCESS;

deinit:
    micro_deinit();
	return work.task_state;
}

/*
    Stub for write action.
*/
//static void write_file(uint8_t *data, int size)
//{
	//FILE *fp;
	//ssize_t r;
	
	//fp = fopen("flash.bin", "w+b");
	
	//r = fwrite(data, sizeof(uint8_t), size, fp);
	
	//printf("%s: wrote %d bytes \n", __func__, r);
//}

/* 
    Application entry point 
*/
int main(int argc, char **argv)
{
	struct sigaction sig_act;
	int ret = 1;

    /* Setup our signal handler. We want to catch SIGINT (Ctrl-C) 
        and SIGTERM. 
    */
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

	if(work.task == FLASH_INTERACTIVE) {
        interactive_action();
        goto close;
    }

	ret = start();

close:

	return ret;
}
