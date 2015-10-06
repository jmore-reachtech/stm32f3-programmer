#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <errno.h>

#include "stm32.h"

#ifdef DEBUG
#define LOG(format, ...) printf(format "\n" , ##__VA_ARGS__);
#else
#define LOG(format, ...)
#endif

stm32_err_t stm_get_ack(struct serial_port_options *opts)
{
	uint8_t byte;
	
	serial_read(opts, &byte, 1);
	
	LOG("read 0x%02X", byte);

	if(byte == STM_ACK) {
		return STM32_ERR_OK;
	}
	
	if(byte == STM_NACK) {
		return STM32_ERR_NACK;
	}

	return STM32_ERR_UNKNOWN;
}

int stm_init_seq(struct serial_port_options *opts)
{
	uint8_t cmd = STM_INIT;
	ssize_t r;
	
	LOG("%s: writing 0x%X to stm",__func__, cmd);
	r = serial_write(opts, &cmd, 1);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}

	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	return 0;
}

int stm_get_cmds(struct serial_port_options *opts)
{
	uint8_t cmd[2], buf[32];
	ssize_t r;
	int i;
	
	cmd[0] = STM_CMD_GET;
	cmd[1] = STM_CMD_GET ^ 0xFF;
	
	LOG("%s: writing 0x%02X to stm",__func__, cmd[0]);
	r = serial_write(opts, &cmd, 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	LOG("%s: getting byte count",__func__);
	serial_read(opts, buf, 1);
	LOG("%s: getting comamnds %d",__func__, buf[0]);
	serial_read(opts, buf, buf[0] + 1);
	
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	for(i = 0; i < 12; i++) {
		LOG("%s: cmd 0x%02X",__func__, buf[i]);
	}
	
	return 0;
}


int stm_erase_mem(struct serial_port_options *opts)
{
	uint8_t cmd[2];
	uint8_t buf[3];
	ssize_t r;
	
	cmd[0] = STM_CMD_ERASE_MEM_EXT;
	cmd[1] = STM_CMD_ERASE_MEM_EXT ^ 0xFF;
	
	buf[0] = 0xFF;
	buf[1] = 0xFF;
	buf[2] = 0x00;
	
	LOG("%s: writing 0x%02X to stm",__func__, cmd[0]);
	r = serial_write(opts, &cmd, 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	LOG("%s: writing 0x%02X%02X to stm",__func__, buf[0], buf[1]);
	//r = write(fd_tty, &buf, 3);
	r = serial_write(opts, &buf, 3);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	return 0;
}

int stm_read_mem(struct serial_port_options *opts, uint32_t address, 
        uint8_t *data, unsigned int len )
{
	uint8_t cmd[2];
	uint8_t buf[5];
	ssize_t r;
	
	cmd[0] = STM_CMD_READ_MEM;
	cmd[1] = STM_CMD_READ_MEM ^ 0xFF;
	
	buf[0] = address >> 24;
	buf[1] = (address >> 16) & 0xFF;
	buf[2] = (address >> 8) & 0xFF;
	buf[3] = address & 0xFF;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	
	LOG("%s: writing 0x%02X to stm",__func__, cmd[0]);
	r = serial_write(opts, &cmd, 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	LOG("%s: writing address 0x%08X to stm",__func__, address);
	r = serial_write(opts, &buf, 5);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	cmd[0] = len - 1;
	cmd[1] = (len - 1) ^ 0xFF;
	LOG("%s: writing len %d to stm",__func__, len);
	r = serial_write(opts, &cmd, 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	serial_read(opts, data, len);

	return 0;
}

int stm_write_mem(struct serial_port_options *opts, uint32_t address, 
        uint8_t data[], unsigned int len)
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

	LOG("%s: writing cmd 0x%02X to stm",__func__, cmd[0]);
	r = serial_write(opts, &cmd, 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}

	LOG("%s: writing address 0x%08X to stm",__func__, address);
	r = serial_write(opts, &buf, 5);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}
	
	aligned_len = (len + 3) & ~3;
	LOG("%s: aligned len %d", __func__, aligned_len);
	
	buf[0] = len -1;
	cs = buf[0];
	for(i = 0; i < len; i++) {
		cs ^= data[i];
		buf[i + 1] = data[i];
	}
	
	buf[len + 1] = cs;
	LOG("%s: writing data to stm",__func__);
	r = serial_write(opts, &buf, len + 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}

	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}

	return 0;
}

int stm_get_id(struct serial_port_options *opts)
{
	uint8_t ver[32];
	uint8_t cmd[2];
	ssize_t r;
	
	cmd[0] = STM_CMD_GET_ID;
	cmd[1] = STM_CMD_GET_ID ^ 0xFF;
	
	LOG("%s: writing 0x%02X to stm",__func__, cmd[0]);
	r = serial_write(opts, &cmd, 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}

	serial_read(opts, &ver,3);
	LOG("%s: ID 0x%02X%02X",__func__, ver[1], ver[2]);

	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}

	return 0;
}

int stm_go(struct serial_port_options *opts, uint32_t address)
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
	
	LOG("%s: writing 0x%02X to stm",__func__, cmd[0]);
	r = serial_write(opts, &cmd, 2);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}
	if( stm_get_ack(opts) != STM32_ERR_OK) {
		LOG("%s: No ACK!", __func__);
		return 1;
	}

	LOG("%s: writing address 0x%08X to stm",__func__, address);
	r = serial_write(opts, &buf, 5);
	if(r < 1) {
		LOG("%s: write failed!", __func__);
		return 1;
	}

	return 0;
}
