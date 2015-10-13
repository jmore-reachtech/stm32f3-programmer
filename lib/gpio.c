#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "linux/i2c-dev-user.h"
#include "gpio.h"

/* Uncomment for full debugging */
//#define DEBUG
#ifdef DEBUG
#define LOG(format, ...) printf(format "\n" , ##__VA_ARGS__);
#else
#define LOG(format, ...)
#endif

static int fd = 0;


/* 
    Set up the GPIO for the reset and boot pin on the STM32 
*/
int gpio_init(void)
{
	int rv = 0;
	
    /* Open the i2c device */
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
	
    /* Use GPIO 2 and 3, pins 1 and 2 on J22 */
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


/* 
    Reset the GPIO port 
*/
void gpio_deinit(void)
{
	int rv = 0;
    
    /* Set all pins to input */
	rv = i2c_smbus_write_byte_data(fd, I2C_CTRL_REG, 0xFF);
	if(rv < 0) {
		LOG("smbus write failed!");
	}
    /* Set all pins to low */
	rv = i2c_smbus_write_byte_data(fd, I2C_OUT_REG, 0x00);
	if(rv < 0) {
		LOG("smbus write failed!");
	}
	
	if(fd) {
		close(fd);
	}
}

/* 
    Toggle the boot pin, LOW or HIGH 
*/
void gpio_toggle_boot(pin_state p)
{
	int rv = 0, reg = 0;
	
    /* read the GPIO out reg */
	reg = i2c_smbus_read_byte_data(fd, I2C_OUT_REG);
	if(reg < 0) {
		LOG("smbus read failed!");
	}

    /* Clear or set the GPIO pin */
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

/* 
    Toggle the reset pin, LOW or HIGH 
*/
void gpio_toggle_reset(pin_state p)
{
	int rv = 0, reg = 0;

    /* read the GPIO out reg */
	reg = i2c_smbus_read_byte_data(fd, I2C_OUT_REG);
	if(reg < 0) {
		LOG("smbus read failed!");
	}

    /* Clear or set the GPIO pin */
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
