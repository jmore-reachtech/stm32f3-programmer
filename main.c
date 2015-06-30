#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <poll.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <linux/serial.h>

#include "linux/i2c-dev-user.h"
#include "main.h"

static int fd_gpio = 0;
static int fd_tty = 0;

static void tio_init(struct termios *t);
static int gpio_init(void);
static void gpio_deinit(void);
static void toggle_boot(pin_state p);
static void toggle_reset(pin_state p);
static void tty_init(void);
static void tty_deinit(void);
static void sig_handler(int sig, siginfo_t *siginfo, void *context);
static void read_file(char *path);
static void write_file(uint8_t *data, int size);
static int serial_read(void *buf, size_t nbyte);
static stm32_err_t stm_get_ack(void);
static int stm_init_seq(void);
static int stm_get_cmds(void);
static int stm_erase_mem(void);
static int stm_read_mem(uint32_t address, unsigned int len);
static int stm_write_mem(uint32_t address, uint8_t data[], unsigned int len);
static int stm_get_id(void);
static int stm_go(uint32_t address);
static void start(void);

static void tio_init(struct termios *t)
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

static int gpio_init(void)
{
	int rv = 0;
	
	fd_gpio = open(I2C_DEV, O_RDWR);
	if(fd_gpio < 0) {
		printf("open %s failed! \n", I2C_DEV);
		return -ENODEV;
	}
	
	rv = ioctl(fd_gpio, I2C_SLAVE, I2C_ADDR);
	if(rv < 0) {
		printf("slave ioctl 0x%02X failed! \n", I2C_ADDR);
		return -ENODEV;
	}
	
	rv = i2c_smbus_write_byte_data(fd_gpio, I2C_CTRL_REG, 0xF3);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
	rv = i2c_smbus_write_byte_data(fd_gpio, I2C_OUT_REG, 0x08);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
	
	rv = i2c_smbus_read_byte_data(fd_gpio, I2C_CTRL_REG);
	if(rv < 0) {
		printf("smbus read failed! \n");
	}
	
	rv = i2c_smbus_read_byte_data(fd_gpio, I2C_OUT_REG);
	if(rv < 0) {
		printf("smbus read failed! \n");
	}
	
	return 0;
}

static void gpio_deinit(void)
{
	int rv = 0;
	rv = i2c_smbus_write_byte_data(fd_gpio, I2C_CTRL_REG, 0xFF);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
	rv = i2c_smbus_write_byte_data(fd_gpio, I2C_OUT_REG, 0x00);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
	
	if(fd_gpio) {
		close(fd_gpio);
	}
}

static void toggle_boot(pin_state p)
{
	int rv = 0, reg = 0;
	
	reg = i2c_smbus_read_byte_data(fd_gpio, I2C_OUT_REG);
	if(reg < 0) {
		printf("smbus read failed! \n");
	}

	switch (p) {
		case LOW:
			reg &= ~(1 << 2);
			break;
		case HIGH:
			reg |= 1 << 2;
			break;
		default:
			printf("invalid case %d \n", p);
	}
	
	rv = i2c_smbus_write_byte_data(fd_gpio, I2C_OUT_REG, reg);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
}

static void toggle_reset(pin_state p)
{
	int rv = 0, reg = 0;
	reg = i2c_smbus_read_byte_data(fd_gpio, I2C_OUT_REG);
	if(reg < 0) {
		printf("smbus read failed! \n");
	}

	switch (p) {
		case LOW:
			reg &= ~(1 << 3);
			break;
		case HIGH:
			reg |= 1 << 3;			
			break;
		default:
			printf("invalid case %d \n", p);
	}
	
	rv = i2c_smbus_write_byte_data(fd_gpio, I2C_OUT_REG, reg);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
}

static void tty_init(void)
{
	struct termios tio;
	
	fd_tty = open(TTY_DEV, O_RDWR | O_NOCTTY);
	if (fd_tty == -1) {
		/* Could not open the port. */
		printf("open_port: Unable to open %s - \n", TTY_DEV);
		return;
	}
    
    tio_init(&tio);
    tcflush(fd_tty, TCIFLUSH);
    cfsetospeed(&tio, B57600);
	cfsetispeed(&tio, B57600);
	tcsetattr(fd_tty, TCSANOW, &tio);
}

static void tty_deinit(void)
{
	if(fd_tty) {
		close(fd_tty);
	}
}


static void sig_handler(int sig, siginfo_t *siginfo, void *context)
{
	toggle_boot(LOW);
	sleep(1);
	toggle_reset(LOW);
	sleep(1);
	toggle_reset(HIGH);
	sleep(1);

	gpio_deinit();
	tty_deinit();
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

static void write_file(uint8_t *data, int size)
{
	FILE *fp;
	ssize_t r;
	
	fp = fopen("flash.bin", "w+b");
	
	r = fwrite(data, sizeof(uint8_t), size, fp);
	
	printf("%s: wrote %d bytes \n", __func__, r);
}

static int serial_read(void *buf, size_t nbyte)
{
	ssize_t r;
	uint8_t *pos = (uint8_t *)buf;

	printf("%s: reading %d bytes \n", __func__, nbyte);
	while (nbyte) {
		r = read(fd_tty, pos, nbyte);
		if (r == 0)
			return 1;
		if (r < 0)
			return 1;

		nbyte -= r;
		pos += r;
	}
	return 0;
}

static stm32_err_t stm_get_ack(void)
{
	uint8_t byte;
	ssize_t r;
	
	r = read(fd_tty, &byte, 1);
	if(r == -1) {
		perror("Error reading tty");
		return 1;
	}
	
	if(byte == STM_ACK) {
		return STM32_ERR_OK;
	}
	
	if(byte == STM_NACK) {
		return STM32_ERR_NACK;
	}

	return STM32_ERR_UNKNOWN;
}

static int stm_init_seq(void)
{
	uint8_t cmd = STM_INIT;
	ssize_t r;
	
	printf("%s: writing 0x%X to stm \n",__func__, cmd);
	r = write(fd_tty, &cmd, 1);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}

	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	return 0;
}

static int stm_get_cmds(void)
{
	uint8_t cmd[2], buf[32];
	ssize_t r;
	int i;
	
	cmd[0] = STM_CMD_GET;
	cmd[1] = STM_CMD_GET ^ 0xFF;
	
	printf("%s: writing 0x%02X to stm \n",__func__, cmd[0]);
	r = write(fd_tty, &cmd, 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	printf("%s: getting byte count \n",__func__);
	serial_read(buf, 1);
	printf("%s: getting comamnds %d \n",__func__, buf[0]);
	serial_read(buf, buf[0] + 1);
	
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	for(i = 0; i < 12; i++) {
		printf("%s: cmd 0x%02X \n",__func__, buf[i]);
	}
	
	return 0;
}


static int stm_erase_mem(void)
{
	uint8_t cmd[2];
	uint8_t buf[3];
	ssize_t r;
	
	cmd[0] = STM_CMD_ERASE_MEM_EXT;
	cmd[1] = STM_CMD_ERASE_MEM_EXT ^ 0xFF;
	
	buf[0] = 0xFF;
	buf[1] = 0xFF;
	buf[2] = 0x00;
	
	printf("%s: writing 0x%02X to stm \n",__func__, cmd[0]);
	r = write(fd_tty, &cmd, 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	printf("%s: writing 0x%02X%02X to stm \n",__func__, buf[0], buf[1]);
	r = write(fd_tty, &buf, 3);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	return 0;
}

static int stm_read_mem(uint32_t address, unsigned int len)
{
	uint8_t cmd[2];
	uint8_t buf[5];
	uint8_t data[256];
	ssize_t r;
	
	cmd[0] = STM_CMD_READ_MEM;
	cmd[1] = STM_CMD_READ_MEM ^ 0xFF;
	
	buf[0] = address >> 24;
	buf[1] = (address >> 16) & 0xFF;
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = address & 0xFF;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	
	printf("%s: writing 0x%02X to stm \n",__func__, cmd[0]);
	r = write(fd_tty, &cmd, 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	printf("%s: writing address 0x%08X to stm \n",__func__, address);
	r = write(fd_tty, &buf, 5);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	cmd[0] = len - 1;
	cmd[1] = (len - 1) ^ 0xFF;
	printf("%s: writing len %d to stm \n",__func__, len);
	r = write(fd_tty, &cmd, 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	serial_read(&data,len -1);

	write_file(data, 256);

	return 0;
}

static int stm_write_mem(uint32_t address, uint8_t data[], unsigned int len)
{
	uint8_t cs;
	uint8_t cmd[2];
	uint8_t buf[256 + 2];
	ssize_t r;
	unsigned int i, aligned_len;

	cmd[0] = STM_CMD_WRITE_MEM;
	cmd[1] = STM_CMD_WRITE_MEM ^ 0xFF;

	buf[0] = address >> 24;
	buf[1] = (address >> 16) & 0xFF;
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = address & 0xFF;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];

	printf("%s: writing cmd 0x%02X to stm \n",__func__, cmd[0]);
	r = write(fd_tty, &cmd, 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}

	printf("%s: writing address 0x%08X to stm \n",__func__, address);
	r = write(fd_tty, &buf, 5);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}
	
	aligned_len = (len + 3) & ~3;
	printf("%s: aligned len %d \n", __func__, aligned_len);
	
	buf[0] = len -1;
	cs = buf[0];
	for(i = 0; i < len; i++) {
		cs ^= data[i];
		buf[i + 1] = data[i];
		//printf("%s: cs = 0x%02X; data = 0x%02X \n", __func__, cs, data[i]);
	}
	buf[len + 1] = cs;
	printf("%s: writing data to stm \n",__func__);
	r = write(fd_tty, &buf, len + 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}

	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}

	return 0;
}

static int stm_get_id(void)
{
	uint8_t ver[32];
	uint8_t cmd[2];
	ssize_t r;
	
	cmd[0] = STM_CMD_GET_ID;
	cmd[1] = STM_CMD_GET_ID ^ 0xFF;
	
	printf("%s: writing 0x%02X to stm \n",__func__, cmd[0]);
	r = write(fd_tty, &cmd, 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}

	serial_read(&ver,3);
	printf("%s: ID 0x%02X%02X \n",__func__, ver[1], ver[2]);

	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}

	return 0;
}

static int stm_go(uint32_t address)
{
	uint8_t cmd[2];
	uint8_t buf[5];
	ssize_t r;
	
	cmd[0] = STM_CMD_GO;
	cmd[1] = STM_CMD_GO ^ 0xFF;
	
	buf[0] = address >> 24;
	buf[1] = (address >> 16) & 0xFF;
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = address & 0xFF;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	
	printf("%s: writing 0x%02X to stm \n",__func__, cmd[0]);
	r = write(fd_tty, &cmd, 2);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	if( stm_get_ack() != STM32_ERR_OK) {
		printf("%s: No ACK! \n", __func__);
		return 1;
	}

	printf("%s: writing address 0x%08X to stm \n",__func__, address);
	r = write(fd_tty, &buf, 5);
	if(r < 1) {
		printf("%s: write failed! \n", __func__);
		return 1;
	}
	
	printf("%s: sleeping \n",__func__);
	sleep(10);

	return 0;
}

static void start(void)
{	
	stm_init_seq();

	stm_get_id();

	//stm_get_cmds();

	stm_erase_mem();

	sleep(2);
	
	read_file("main.bin");

	//stm_write_mem(0x08000000);

	//stm_go(0x08000000);

	//stm_read_mem(0x08000000, 256);
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
	tty_init();
	
	toggle_boot(HIGH);
	sleep(1);
	toggle_reset(LOW);
	sleep(1);
	toggle_reset(HIGH);
	sleep(1);
	
	start();

	toggle_boot(LOW);
	sleep(1);
	toggle_reset(LOW);
	sleep(1);
	toggle_reset(HIGH);
	sleep(1);
	
	gpio_deinit();
	tty_deinit();
		
	return 0;
}
