#ifndef ATH_LUA_RENDER_H
#define ATH_LUA_RENDER_H

#include "ath_lua.h"
#include <athena/render_facade.h>

#define ATHENA_LUA_RENDEROBJECT "RenderObject"

AthenaRenderObject *athena_lua_check_render_object(lua_State *L, int idx);

#endif /* ATH_LUA_RENDER_H */
