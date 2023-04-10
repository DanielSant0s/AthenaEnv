#ifndef DPRINTF_H
#define DPRINTF_H

#ifdef DEBUG
#ifdef __EESIO_PRINTF
    #define dbgprintf(fmt, arg...) sio_printf(fmt, ##arg)
    #define dbgputs(put) sio_puts(put)
#else
    #define dbgprintf(fmt, arg...) printf(fmt, ##arg)
    #define dbgputs(put) puts(put)
#endif //dbgprintf
#endif //DEBUG

#ifndef dbgprintf
#define dbgprintf(x...)
#endif


#endif //DPRINTF_H