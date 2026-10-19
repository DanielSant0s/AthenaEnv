#ifndef ATH_LUA_PHYSICS_H
#define ATH_LUA_PHYSICS_H

#include "ath_lua.h"
#include <physics.h>

AthenaPhysicsGeom *athena_lua_check_geom(lua_State *L, int idx);
AthenaPhysicsBody *athena_lua_check_body(lua_State *L, int idx);
AthenaPhysicsSpace *athena_lua_check_space(lua_State *L, int idx);

#endif /* ATH_LUA_PHYSICS_H */
