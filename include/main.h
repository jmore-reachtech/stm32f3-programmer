#ifndef _MAIN_H
#define _MAIN_H

#define USER_DATA_OFFSET  0x0803F800

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
