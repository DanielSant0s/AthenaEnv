#include <stdarg.h>
#include <stdio.h>
#include <sio.h>

void sio_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char str[256];
    vsnprintf(str, sizeof(str), fmt, args);
    sio_putsn(str);
}