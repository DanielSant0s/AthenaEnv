#include <kernel.h>
#include "include/ee_tools.h"

void RedirectFunction(void* old, void* new) {
    u32* function = old;
    u32 patch = (u32)new;

    FlushCache(2);


    function[0] = 0x08000000+(patch/4);
    function[1] = 0;

    FlushCache(2);

}

void RedirectCall(void* old, void* new) {
    u32* function = old;
    u32 patch = (u32)new;

    FlushCache(2);


    *function = 0x0C000000+(patch/4);

    FlushCache(2);


}