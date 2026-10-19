#ifndef ATH_LUA_GRAPHICS_H
#define ATH_LUA_GRAPHICS_H

#include "ath_lua.h"
#include <athena/image.h>
#include <athena/image_list.h>

#define ATHENA_LUA_IMAGE "Image"

AthenaImage *athena_lua_check_image(lua_State *L, int idx);
AthenaImage *athena_lua_test_image(lua_State *L, int idx);
/* owned == false for images whose memory belongs to the engine (screen buffers). */
void athena_lua_push_image(lua_State *L, AthenaImage *image, bool owned);

AthenaImageList *athena_lua_check_image_list(lua_State *L, int idx);

/* GS ALPHA register from a {a, b, c, d, fix} table (or a small integer). */
uint64_t athena_lua_check_alpha_equation(lua_State *L, int idx);

#endif /* ATH_LUA_GRAPHICS_H */
