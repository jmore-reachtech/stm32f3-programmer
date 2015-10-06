#ifndef _LOG_H
#define _LOG_H

#include <syslog.h>

void log_die_with_system_message(char*);
void log_msg(int level, const char *fmt, ...);

#endif // _LOG_H
