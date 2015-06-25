#ifndef _MAIN_H
#define _MAIN_H

#define TTY_DEV "/dev/ttymxc4"

#define I2C_ADDR 0x3E
#define I2C_DEV "/dev/i2c-0"

#define I2C_INPUT_REG				0x00
#define I2C_OUT_REG					0x01
#define I2C_POLARITY_REG			0x02
#define I2C_CTRL_REG				0x03

typedef enum {LOW = 0, HIGH} pin_state;

#define STM_INIT 				0x7F
#define STM_ACK 				0x79
#define STM_NACK 				0x1F
#define STM_CMD_GET				0x00
#define STM_CMD_GET_VER			0x01
#define STM_CMD_GET_ID			0x02
#define STM_CMD_READ_MEM		0x11
#define STM_CMD_GO				0x21
#define STM_CMD_WRITE_MEM		0x31
#define STM_CMD_ERASE_MEM		0x43
#define STM_CMD_ERASE_MEM_EXT	0x44
#define STM_CMD_WRITE_PROTECT	0x63
#define STM_CMD_WRITE_UNPROTECT	0x73
#define STM_CMD_READ_PROTECT	0x82
#define STM_CMD_READ_UNPROTECT	0x92


typedef enum {
	STM32_ERR_OK = 0,
	STM32_ERR_UNKNOWN,	/* Generic error */
	STM32_ERR_NACK,
	STM32_ERR_NO_CMD,	/* Command not available in bootloader */
} stm32_err_t;

#define GPIO_RESET_MASK 0xF7
#define GPIO_BOOTP_MASK 0xFB


#endif // _MAIN_H
