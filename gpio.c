#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "linux/i2c-dev-user.h"
#include "gpio.h"

static int fd = 0;

int gpio_init(void)
{
	int rv = 0;
	
	fd = open(I2C_DEV, O_RDWR);
	if(fd < 0) {
		printf("open %s failed! \n", I2C_DEV);
		return -ENODEV;
	}
	
	rv = ioctl(fd, I2C_SLAVE, I2C_ADDR);
	if(rv < 0) {
		printf("slave ioctl 0x%02X failed! \n", I2C_ADDR);
		return -ENODEV;
	}
	
	rv = i2c_smbus_write_byte_data(fd, I2C_CTRL_REG, 0xF3);
	if(rv < 0) {
		printf("smbus write failed! \n");
		return -1;
	}
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, 0x08);
	if(rv < 0) {
		printf("smbus write failed! \n");
		return -1;
	}
	
	rv = i2c_smbus_read_byte_data(fd, I2C_CTRL_REG);
	if(rv < 0) {
		printf("smbus read failed! \n");
		return -1;
	}
	
	rv = i2c_smbus_read_byte_data(fd, I2C_OUT_REG);
	if(rv < 0) {
		printf("smbus read failed! \n");
		return -1;
	}
	
	return 0;
}

void gpio_deinit(void)
{
	int rv = 0;
	rv = i2c_smbus_write_byte_data(fd, I2C_CTRL_REG, 0xFF);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, 0x00);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
	
	if(fd) {
		close(fd);
	}
}

void gpio_toggle_boot(pin_state p)
{
	int rv = 0, reg = 0;
	
	reg = i2c_smbus_read_byte_data(fd, I2C_OUT_REG);
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
	
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, reg);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
}

void gpio_toggle_reset(pin_state p)
{
	int rv = 0, reg = 0;
	reg = i2c_smbus_read_byte_data(fd, I2C_OUT_REG);
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
	
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, reg);
	if(rv < 0) {
		printf("smbus write failed! \n");
	}
}
