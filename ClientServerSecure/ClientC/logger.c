#pragma once
#define LOGGING_ENABLED 1
#include "logger.h"
#include <stdarg.h>
#include <stdio.h>

void log(char* format, ...) {
	if (LOGGING_ENABLED) {
        printf("|LOGGER|");
        va_list argptr;
        va_start(argptr, format);
        vfprintf(stdout, format, argptr);
        va_end(argptr);
	}
}

