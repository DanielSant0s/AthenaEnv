#ifndef ATH_LUA_MATH_H
#define ATH_LUA_MATH_H

#include "ath_lua.h"
#include <athena/vec3.h>
#include <athena/vec4.h>
#include <athena/matrix.h>

#define ATHENA_LUA_VEC4 "Vector4"
#define ATHENA_LUA_MATRIX4 "Matrix4"

AthenaVec3 *athena_lua_push_vec3(lua_State *L, float x, float y, float z);
AthenaVec3 *athena_lua_check_vec3(lua_State *L, int idx);
AthenaVec3 *athena_lua_test_vec3(lua_State *L, int idx);

AthenaVec4 *athena_lua_check_vec4(lua_State *L, int idx);
AthenaMatrix4 *athena_lua_check_matrix(lua_State *L, int idx);

/* Views over engine memory (16-byte aligned VECTOR / MATRIX); the object at
 * `owner` is kept alive while the view exists. */
void athena_lua_push_vec4_view(lua_State *L, float *data, int owner);
void athena_lua_push_matrix_view(lua_State *L, float *data, int owner);
void athena_lua_push_matrix_copy(lua_State *L, const float *data);

#endif /* ATH_LUA_MATH_H */
