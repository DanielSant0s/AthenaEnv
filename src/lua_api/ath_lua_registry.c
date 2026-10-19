#include "ath_lua.h"

/* Module openers. Each one sets a global and package.loaded[name], so both
 * `Screen.flip()` and `local Screen = require "Screen"` work. */
void athena_lua_open_system(lua_State *L);
void athena_lua_open_pads(lua_State *L);
void athena_lua_open_timer(lua_State *L);
void athena_lua_open_buffer(lua_State *L);
void athena_lua_open_math(lua_State *L);
void athena_lua_open_sys(lua_State *L);
void athena_lua_open_input(lua_State *L);

#ifdef ATHENA_GRAPHICS
void athena_lua_open_color(lua_State *L);
void athena_lua_open_draw(lua_State *L);
void athena_lua_open_graphics(lua_State *L);
void athena_lua_open_render(lua_State *L);
void athena_lua_open_fx(lua_State *L);
#endif

#ifdef ATHENA_PHYSICS
void athena_lua_open_physics(lua_State *L);
#endif

/* Built in only when the matching STATIC_* flag is set; otherwise they come
 * from <name>_lua.erl through the lazy globals in prelude.lua. */
#ifdef ATHENA_NETWORK
void athena_lua_open_network(lua_State *L);
#endif
#ifdef ATHENA_ARCHIVE
void athena_lua_open_archive(lua_State *L);
#endif
#ifdef ATHENA_MPEG_VIDEO
void athena_lua_open_video(lua_State *L);
#endif

typedef void (*athena_lua_opener)(lua_State *L);

static const athena_lua_opener openers[] = {
    athena_lua_open_system,
    athena_lua_open_pads,
    athena_lua_open_timer,
    athena_lua_open_buffer,
    athena_lua_open_math,
    athena_lua_open_sys,
    athena_lua_open_input,
#ifdef ATHENA_GRAPHICS
    athena_lua_open_color,
    athena_lua_open_draw,
    athena_lua_open_graphics,
    athena_lua_open_render,
    athena_lua_open_fx,
#endif
#ifdef ATHENA_PHYSICS
    athena_lua_open_physics,
#endif
#ifdef ATHENA_NETWORK
    athena_lua_open_network,
#endif
#ifdef ATHENA_ARCHIVE
    athena_lua_open_archive,
#endif
#ifdef ATHENA_MPEG_VIDEO
    athena_lua_open_video,
#endif
};

void athena_lua_register_modules(lua_State *L)
{
    for (unsigned i = 0; i < countof(openers); i++)
        openers[i](L);
}
