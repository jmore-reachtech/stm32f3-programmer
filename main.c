#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>

#include "main.h"
#include "serial.h"
#include "gpio.h"
#include "stm32.h"

static void read_file(char *path);
static void start(void);

static void sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	gpio_toggle_boot(LOW);
	sleep(1);
	gpio_toggle_reset(LOW);
	sleep(1);
	gpio_toggle_reset(HIGH);
	sleep(1);

	gpio_deinit();
	serial_deinit();
}

static void read_file(char *path)
{
	FILE *fp;
	ssize_t r;
	long size;
	uint8_t tmp[MAX_RW_SIZE];
	uint32_t addr = STM_FLASH_BASE;
	unsigned int i;

	fp = fopen("main.bin", "rb");

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
}

static void start(void)
{	
	stm_init_seq();

	stm_get_id();

	//stm_get_cmds();

	stm_erase_mem();

	sleep(2);
	
	read_file("main.bin");

	//stm_write_mem(STM_FLASH_BASE);

	//stm_go(STM_FLASH_BASE);

	//stm_read_mem(STM_FLASH_BASE, 256);
}

int main(int argc, char **argv)
{
	struct sigaction sig_act;

	sig_act.sa_sigaction = sig_handler;
	sig_act.sa_flags = SA_SIGINFO;
	if (sigaction(SIGTERM, &sig_act, NULL) < 0) {
		perror ("sigaction");
		return 1;
	}
	if (sigaction(SIGINT, &sig_act, NULL) < 0) {
		perror ("sigaction");
		return 1;
	}
	
	gpio_init();
	serial_init();
	
	gpio_toggle_boot(HIGH);
	sleep(1);
	gpio_toggle_reset(LOW);
	sleep(1);
	gpio_toggle_reset(HIGH);
	sleep(1);
	
	start();

	gpio_toggle_boot(LOW);
	sleep(1);
	gpio_toggle_reset(LOW);
	sleep(1);
	gpio_toggle_reset(HIGH);
	sleep(1);
	
	gpio_deinit();
	serial_deinit();
		
	return 0;
}
