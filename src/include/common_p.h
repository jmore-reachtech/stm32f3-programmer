#ifndef _COMMON_H
#define _COMMON_H

#define USER_DATA_OFFSET  0x08000188

#define APP_VERSION     0x00001200

#define VERSION_MAJOR_BM    0x0000F000
#define VERSION_MINOR_BM    0x00000F00
#define VERSION_PATCH_BM    0x000000F0
#define VERSION_REVISION_BM 0x000000F0

#define  VERSION_MAJOR(x) ((x & VERSION_MAJOR_BM) >> 12)
#define  VERSION_MINOR(x) ((x & VERSION_MINOR_BM) >> 8)
#define  VERSION_PATCH(x) ((x & VERSION_PATCH_BM) >> 4)
#define  VERSION_REVISION(x) ((x & VERSION_REVISION_BM) >> 0)

typedef enum {
	FLASH_READ = 0,
	FLASH_WRITE,
	FLASH_CHECK_VER,
	FLASH_HELP,
	FLASH_NONE,
	FLASH_QUERY,
    FLASH_VERSION,
    FLASH_INTERACTIVE,
} work_action;

typedef enum {
	TASK_SUCCESS = 0,
	TASK_IDLE,
	TASK_START,
	TASK_ACTIVE,
	TASK_FAILED,
} task_state_t;

typedef enum {
	MATCH = 0,
	MISMATCH,
	UNCHECKED,
} version_check;

typedef enum {
    CMD_EXIT = 0,
    CMD_HELP,
    CMD_MICRO_VER,
    CMD_APP_VER,
    CMD_UPDATE,
    CMD_FIRMWARE,
    CMD_STATUS,
    CMD_UNKNOWN,
} cmd_action;

#endif // _COMMON_H
