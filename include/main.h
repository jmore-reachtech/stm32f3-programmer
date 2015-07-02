#ifndef _MAIN_H
#define _MAIN_H

typedef enum {
	FLASH_READ = 0,
	FLASH_WRITE,
	FLASH_CHECK_VER,
	FLASH_HELP,
	FLASH_NONE,
} work_action;

typedef enum {
	SUCCESS = 0,
	IDLE,
	START,
	INITED,
	ACTIVE,
	FAILED,
} work_state;

#endif // _MAIN_H
