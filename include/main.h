#ifndef _MAIN_H
#define _MAIN_H

#define USER_DATA_OFFSET 0x0803F800
#define MICRO_VERSION 0xDEADBEEF

typedef enum {
	FLASH_READ = 0,
	FLASH_WRITE,
	FLASH_CHECK_VER,
	FLASH_HELP,
	FLASH_NONE,
	FLASH_QUERY,
} work_action;

typedef enum {
	SUCCESS = 0,
	IDLE,
	START,
	INITED,
	ACTIVE,
	FAILED,
} work_state;

typedef enum {
	MATCH = 0,
	MISMATCH,
	UNCHECKED,
} version_check;

#endif // _MAIN_H
