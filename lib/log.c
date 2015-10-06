#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "log.h"

void log_die_with_system_message(char* msg)
{
   	perror(msg);
	exit (1); 
}

void log_msg(int level, const char *fmt, ...)
{
    
}

