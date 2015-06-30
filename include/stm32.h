#ifndef _STM32_H
#define _STM32_H

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

#define STM_FLASH_BASE			0x08000000
#define MAX_RW_SIZE				0x100

typedef enum {
	STM32_ERR_OK = 0,
	STM32_ERR_UNKNOWN,	/* Generic error */
	STM32_ERR_NACK,
	STM32_ERR_NO_CMD,	/* Command not available in bootloader */
} stm32_err_t;

#define GPIO_RESET_MASK 0xF7
#define GPIO_BOOTP_MASK 0xFB

stm32_err_t stm_get_ack(void);
int stm_init_seq(void);
int stm_get_cmds(void);
int stm_erase_mem(void);
int stm_read_mem(uint32_t address, unsigned int len);
int stm_write_mem(uint32_t address, uint8_t data[], unsigned int len);
int stm_get_id(void);
int stm_go(uint32_t address);

#endif // _STM32_H
