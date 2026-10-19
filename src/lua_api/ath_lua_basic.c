#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include "ath_lua.h"
#include <athena/timer.h>

#ifdef ATHENA_GRAPHICS
#include <athena/color.h>
#include <athena/draw.h>
#endif

/* ---------------------------------------------------------------- Timer --
 * Timer.new() returns an object; both t:getTime() and Timer.getTime(t) work,
 * so code written against the JS handle-style API ports unchanged. */

#define TIMER_CLASS "Timer"

typedef struct {
    AthenaTimer *timer;
} LuaTimer;

static AthenaTimer *check_timer(lua_State *L, int idx)
{
    LuaTimer *t = athena_lua_check_object(L, idx, TIMER_CLASS);
    if (!t->timer)
        luaL_error(L, "timer was destroyed");
    return t->timer;
}

static int l_timer_new(lua_State *L)
{
    AthenaTimer *timer = athena_timer_create();
    if (!timer)
        return luaL_error(L, "could not create timer");
    LuaTimer *t = athena_lua_new_object(L, TIMER_CLASS, sizeof(LuaTimer));
    t->timer = timer;
    return 1;
}

static int l_timer_destroy(lua_State *L)
{
    LuaTimer *t = athena_lua_check_object(L, 1, TIMER_CLASS);
    if (t->timer) {
        athena_timer_destroy(t->timer);
        t->timer = NULL;
    }
    return 0;
}

static int l_timer_get_time(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)athena_timer_get_time(check_timer(L, 1)));
    return 1;
}

static int l_timer_set_time(lua_State *L)
{
    athena_timer_set_time(check_timer(L, 1), (clock_t)athena_lua_checku32(L, 2));
    return 0;
}

static int l_timer_pause(lua_State *L)
{
    athena_timer_pause(check_timer(L, 1));
    return 0;
}

static int l_timer_resume(lua_State *L)
{
    athena_timer_resume(check_timer(L, 1));
    return 0;
}

static int l_timer_reset(lua_State *L)
{
    athena_timer_reset(check_timer(L, 1));
    return 0;
}

static int l_timer_is_playing(lua_State *L)
{
    lua_pushboolean(L, athena_timer_is_playing(check_timer(L, 1)));
    return 1;
}

static const luaL_Reg timer_methods[] = {
    { "getTime", l_timer_get_time },
    { "setTime", l_timer_set_time },
    { "destroy", l_timer_destroy },
    { "pause", l_timer_pause },
    { "resume", l_timer_resume },
    { "reset", l_timer_reset },
    { "isPlaying", l_timer_is_playing },
    { NULL, NULL }
};

static const athena_lua_class timer_class = {
    .name = TIMER_CLASS,
    .methods = timer_methods,
    .gc = l_timer_destroy,
};

void athena_lua_open_timer(lua_State *L)
{
    athena_lua_register_class(L, &timer_class);
    athena_lua_new_module(L, timer_methods, NULL, l_timer_new);
    athena_lua_set_global_module(L, "Timer");
}

/* --------------------------------------------------------------- Buffer --
 * Lua has no ArrayBuffer; Buffer is a fixed-size byte array with typed
 * accessors, accepted by every API that takes binary data in JS. */

static AthenaLuaBuffer *check_buffer(lua_State *L, int idx)
{
    return athena_lua_check_object(L, idx, ATHENA_LUA_BUFFER);
}

AthenaLuaBuffer *athena_lua_push_buffer(lua_State *L, void *data, uint32_t size, bool owned)
{
    AthenaLuaBuffer *b = athena_lua_new_object(L, ATHENA_LUA_BUFFER, sizeof(AthenaLuaBuffer));
    b->data = data;
    b->size = size;
    b->owned = owned;
    return b;
}

const uint8_t *athena_lua_to_bytes(lua_State *L, int idx, uint32_t *size)
{
    AthenaLuaBuffer *b = athena_lua_test_object(L, idx, ATHENA_LUA_BUFFER);
    if (b) {
        *size = b->size;
        return b->data;
    }

    if (lua_type(L, idx) == LUA_TSTRING) {
        size_t len;
        const char *s = lua_tolstring(L, idx, &len);
        *size = (uint32_t)len;
        return (const uint8_t *)s;
    }

    *size = 0;
    return NULL;
}

/* 16-byte aligned so buffers can feed DMA/VU paths (sprites, vertices). */
static uint8_t *buffer_alloc(uint32_t size)
{
    uint32_t padded = (size + 15u) & ~15u;
    uint8_t *data = memalign(16, padded ? padded : 16);
    if (data)
        memset(data, 0, padded ? padded : 16);
    return data;
}

/* Buffer.new(size) or Buffer.new(string) */
static int l_buffer_new(lua_State *L)
{
    if (lua_type(L, 1) == LUA_TSTRING) {
        size_t len;
        const char *s = lua_tolstring(L, 1, &len);
        uint8_t *data = buffer_alloc(len);
        if (!data)
            return luaL_error(L, "out of memory");
        memcpy(data, s, len);
        athena_lua_push_buffer(L, data, len, true);
        return 1;
    }

    uint32_t size = athena_lua_checku32(L, 1);
    uint8_t *data = buffer_alloc(size);
    if (!data)
        return luaL_error(L, "out of memory");
    athena_lua_push_buffer(L, data, size, true);
    return 1;
}

static int l_buffer_gc(lua_State *L)
{
    AthenaLuaBuffer *b = check_buffer(L, 1);
    if (b->owned && b->data)
        free(b->data);
    b->data = NULL;
    b->size = 0;
    return 0;
}

static int l_buffer_len(lua_State *L)
{
    lua_pushinteger(L, check_buffer(L, 1)->size);
    return 1;
}

/* Offsets are byte offsets, 0-based, like DataView. */
static void *buffer_at(lua_State *L, AthenaLuaBuffer *b, int idx, uint32_t width)
{
    lua_Integer off = luaL_checkinteger(L, idx);
    if (off < 0 || (uint32_t)off + width > b->size)
        luaL_error(L, "buffer offset %d out of range (size %d)", (int)off, (int)b->size);
    return b->data + off;
}

#define BUFFER_ACCESSORS(suffix, type, push, check) \
    static int l_buffer_get_##suffix(lua_State *L) { \
        AthenaLuaBuffer *b = check_buffer(L, 1); \
        type v; memcpy(&v, buffer_at(L, b, 2, sizeof(type)), sizeof(type)); \
        push(L, v); return 1; } \
    static int l_buffer_set_##suffix(lua_State *L) { \
        AthenaLuaBuffer *b = check_buffer(L, 1); \
        type v = (type)check(L, 3); \
        memcpy(buffer_at(L, b, 2, sizeof(type)), &v, sizeof(type)); return 0; }

BUFFER_ACCESSORS(u8, uint8_t, lua_pushinteger, luaL_checkinteger)
BUFFER_ACCESSORS(i8, int8_t, lua_pushinteger, luaL_checkinteger)
BUFFER_ACCESSORS(u16, uint16_t, lua_pushinteger, luaL_checkinteger)
BUFFER_ACCESSORS(i16, int16_t, lua_pushinteger, luaL_checkinteger)
BUFFER_ACCESSORS(u32, uint32_t, athena_lua_pushu32, athena_lua_checku32)
BUFFER_ACCESSORS(i32, int32_t, lua_pushinteger, luaL_checkinteger)
BUFFER_ACCESSORS(f32, float, lua_pushnumber, luaL_checknumber)

static int l_buffer_tostring(lua_State *L)
{
    AthenaLuaBuffer *b = check_buffer(L, 1);
    lua_Integer from = luaL_optinteger(L, 2, 0);
    lua_Integer len = luaL_optinteger(L, 3, (lua_Integer)b->size - from);
    if (from < 0 || len < 0 || (uint32_t)(from + len) > b->size)
        return luaL_error(L, "range out of buffer");
    lua_pushlstring(L, (const char *)b->data + from, (size_t)len);
    return 1;
}

static int l_buffer_fill(lua_State *L)
{
    AthenaLuaBuffer *b = check_buffer(L, 1);
    memset(b->data, (int)luaL_optinteger(L, 2, 0), b->size);
    return 0;
}

/* buf:copy(src, [dstOffset], [srcOffset], [len]) */
static int l_buffer_copy(lua_State *L)
{
    AthenaLuaBuffer *dst = check_buffer(L, 1);
    uint32_t src_size;
    const uint8_t *src = athena_lua_to_bytes(L, 2, &src_size);
    if (!src)
        return luaL_argerror(L, 2, "Buffer or string expected");

    lua_Integer dst_off = luaL_optinteger(L, 3, 0);
    lua_Integer src_off = luaL_optinteger(L, 4, 0);
    lua_Integer len = luaL_optinteger(L, 5, (lua_Integer)src_size - src_off);
    if (dst_off < 0 || src_off < 0 || len < 0 ||
        (uint32_t)(src_off + len) > src_size || (uint32_t)(dst_off + len) > dst->size)
        return luaL_error(L, "copy range out of bounds");

    memmove(dst->data + dst_off, src + src_off, (size_t)len);
    return 0;
}

static const luaL_Reg buffer_methods[] = {
    { "getU8", l_buffer_get_u8 }, { "setU8", l_buffer_set_u8 },
    { "getI8", l_buffer_get_i8 }, { "setI8", l_buffer_set_i8 },
    { "getU16", l_buffer_get_u16 }, { "setU16", l_buffer_set_u16 },
    { "getI16", l_buffer_get_i16 }, { "setI16", l_buffer_set_i16 },
    { "getU32", l_buffer_get_u32 }, { "setU32", l_buffer_set_u32 },
    { "getI32", l_buffer_get_i32 }, { "setI32", l_buffer_set_i32 },
    { "getF32", l_buffer_get_f32 }, { "setF32", l_buffer_set_f32 },
    { "toString", l_buffer_tostring },
    { "fill", l_buffer_fill },
    { "copy", l_buffer_copy },
    { NULL, NULL }
};

static int get_buffer_size(lua_State *L)
{
    lua_pushinteger(L, check_buffer(L, 1)->size);
    return 1;
}

static const athena_lua_prop buffer_props[] = {
    { "size", get_buffer_size, NULL },
    { "byteLength", get_buffer_size, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg buffer_meta[] = {
    { "__len", l_buffer_len },
    { NULL, NULL }
};

static const athena_lua_class buffer_class = {
    .name = ATHENA_LUA_BUFFER,
    .methods = buffer_methods,
    .props = buffer_props,
    .gc = l_buffer_gc,
    .meta = buffer_meta,
};

void athena_lua_open_buffer(lua_State *L)
{
    athena_lua_register_class(L, &buffer_class);
    athena_lua_new_module(L, NULL, NULL, l_buffer_new);
    athena_lua_set_global_module(L, "Buffer");
}

#ifdef ATHENA_GRAPHICS
/* ---------------------------------------------------------------- Color -- */

static int l_color_new(lua_State *L)
{
    athena_lua_pushu32(L, athena_color_rgba(athena_lua_checku32(L, 1), athena_lua_checku32(L, 2),
                                            athena_lua_checku32(L, 3), athena_lua_optu32(L, 4, 0x80)));
    return 1;
}

#define COLOR_GETSET(ch) \
    static int l_color_get_##ch(lua_State *L) { athena_lua_pushu32(L, athena_color_get_##ch(athena_lua_checku32(L, 1))); return 1; } \
    static int l_color_set_##ch(lua_State *L) { athena_lua_pushu32(L, athena_color_set_##ch(athena_lua_checku32(L, 1), athena_lua_checku32(L, 2))); return 1; }

COLOR_GETSET(r)
COLOR_GETSET(g)
COLOR_GETSET(b)
COLOR_GETSET(a)

static const luaL_Reg color_funcs[] = {
    { "new", l_color_new },
    { "getR", l_color_get_r }, { "getG", l_color_get_g }, { "getB", l_color_get_b }, { "getA", l_color_get_a },
    { "setR", l_color_set_r }, { "setG", l_color_set_g }, { "setB", l_color_set_b }, { "setA", l_color_set_a },
    { NULL, NULL }
};

void athena_lua_open_color(lua_State *L)
{
    athena_lua_new_module(L, color_funcs, NULL, NULL);
    athena_lua_set_global_module(L, "Color");
}

/* ----------------------------------------------------------------- Draw -- */

#define F(i) athena_lua_checkfloat(L, i)
#define C(i) ((Color)athena_lua_checku32(L, i))

static int l_draw_point(lua_State *L)
{
    athena_draw_point(F(1), F(2), C(3));
    return 0;
}

static int l_draw_line(lua_State *L)
{
    athena_draw_line(F(1), F(2), F(3), F(4), C(5));
    return 0;
}

static int l_draw_triangle(lua_State *L)
{
    if (lua_gettop(L) <= 7)
        athena_draw_triangle(F(1), F(2), F(3), F(4), F(5), F(6), C(7));
    else
        athena_draw_triangle_gouraud(F(1), F(2), F(3), F(4), F(5), F(6), C(7), C(8), C(9));
    return 0;
}

static int l_draw_quad(lua_State *L)
{
    if (lua_gettop(L) <= 9)
        athena_draw_quad(F(1), F(2), F(3), F(4), F(5), F(6), F(7), F(8), C(9));
    else
        athena_draw_quad_gouraud(F(1), F(2), F(3), F(4), F(5), F(6), F(7), F(8), C(9), C(10), C(11), C(12));
    return 0;
}

static int l_draw_rect(lua_State *L)
{
    athena_draw_rect(F(1), F(2), F(3), F(4), C(5));
    return 0;
}

static int l_draw_circle(lua_State *L)
{
    bool filled = lua_isnoneornil(L, 5) ? true : athena_lua_tobool(L, 5);
    athena_draw_circle(F(1), F(2), F(3), C(4), filled);
    return 0;
}

#undef F
#undef C

static const luaL_Reg draw_funcs[] = {
    { "point", l_draw_point },
    { "line", l_draw_line },
    { "triangle", l_draw_triangle },
    { "quad", l_draw_quad },
    { "rect", l_draw_rect },
    { "circle", l_draw_circle },
    { NULL, NULL }
};

void athena_lua_open_draw(lua_State *L)
{
    athena_lua_new_module(L, draw_funcs, NULL, NULL);
    athena_lua_set_global_module(L, "Draw");
}
#endif
