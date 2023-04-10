#ifndef DPRINTF_H
#define DPRINTF_H

#ifdef DEBUG
#ifdef __EESIO_PRINTF
    #include <SIOCookie.h>
    extern void sio_printf(const char *fmt, ...); //dont flood log with `-Wimplicit-function-declaration`
    #define dbginit() ee_sio_start(38400, 0, 0, 0, 0)
    #define dbgprintf(fmt, arg...) sio_printf(fmt, ##arg)
    #define dbgputs(put) sio_puts(put)
#else
    #define dbginit()
    #define dbgprintf(fmt, arg...) printf(fmt, ##arg)
    #define dbgputs(put) puts(put)
#endif //dbgprintf
#endif //DEBUG

#ifndef dbgprintf
#define dbgprintf(x...)
#endif

#ifndef dbginit
#define dbginit(x...)
#endif


#endif //DPRINTF_H