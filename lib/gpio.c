#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "linux/i2c-dev-user.h"
#include "gpio.h"

#ifdef DEBUG
#define LOG(format, ...) printf(format "\n" , ##__VA_ARGS__);
#else
#define LOG(format, ...)
#endif

static int fd = 0;

int gpio_init(void)
{
	int rv = 0;
	
	fd = open(I2C_DEV, O_RDWR);
	if(fd < 0) {
		LOG("open %s failed!", I2C_DEV);
		return -ENODEV;
	}
	
	rv = ioctl(fd, I2C_SLAVE, I2C_ADDR);
	if(rv < 0) {
		LOG("slave ioctl 0x%02X failed!", I2C_ADDR);
		return -ENODEV;
	}
	
	rv = i2c_smbus_write_byte_data(fd, I2C_CTRL_REG, 0xF3);
	if(rv < 0) {
		perror("smbus ctrl_reg write failed!");
		return -1;
	}
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, 0x08);
	if(rv < 0) {
		perror("smbus out_reg write failed!");
		return -1;
	}
	
	rv = i2c_smbus_read_byte_data(fd, I2C_CTRL_REG);
	if(rv < 0) {
		perror("smbus ctrl_reg read failed!");
		return -1;
	}
	
	rv = i2c_smbus_read_byte_data(fd, I2C_OUT_REG);
	if(rv < 0) {
		perror("smbus out_reg read failed!");
		return -1;
	}
	
	return 0;
}

void gpio_deinit(void)
{
	int rv = 0;
	rv = i2c_smbus_write_byte_data(fd, I2C_CTRL_REG, 0xFF);
	if(rv < 0) {
		LOG("smbus write failed!");
	}
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, 0x00);
	if(rv < 0) {
		LOG("smbus write failed!");
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
		LOG("smbus read failed!");
	}

	switch (p) {
		case LOW:
			reg &= ~(1 << 2);
			break;
		case HIGH:
			reg |= 1 << 2;
			break;
		default:
			LOG("invalid case %d", p);
	}
	
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, reg);
	if(rv < 0) {
		LOG("smbus write failed!");
	}
}

void gpio_toggle_reset(pin_state p)
{
	int rv = 0, reg = 0;
	reg = i2c_smbus_read_byte_data(fd, I2C_OUT_REG);
	if(reg < 0) {
		LOG("smbus read failed!");
	}

	switch (p) {
		case LOW:
			reg &= ~(1 << 3);
			break;
		case HIGH:
			reg |= 1 << 3;			
			break;
		default:
			LOG("invalid case %d", p);
	}
	
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, reg);
	if(rv < 0) {
		LOG("smbus write failed!");
	}
}
