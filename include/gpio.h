#ifndef _GPIO_H
#define _GPIO_H

#define I2C_ADDR 0x3E
#define I2C_DEV "/dev/i2c-0"

#define I2C_INPUT_REG				0x00
#define I2C_OUT_REG					0x01
#define I2C_POLARITY_REG			0x02
#define I2C_CTRL_REG				0x03

typedef enum {LOW = 0, HIGH} pin_state;

#define GPIO_RESET_MASK 0xF7
#define GPIO_BOOTP_MASK 0xFB

int gpio_init(void);
void gpio_deinit(void);
void gpio_toggle_boot(pin_state p);
void gpio_toggle_reset(pin_state p);

#endif // _GPIO_H
