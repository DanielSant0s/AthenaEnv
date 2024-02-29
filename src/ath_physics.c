#include "ath_env.h"
#include <math.h>
#include <stdio.h>

#include "include/render.h"

static bool overlap(float min1, float max1, float min2, float max2) {
    return max1 >= min2 && max2 >= min1;
}

static bool boxBoxCollide(VECTOR bbox1[8], VECTOR bbox2[8], float coords1[3], float coords2[3], float collision[3]) {
    for (int i = 0; i < 3; ++i) {
        float min1 = bbox1[0][i] + coords1[i], max1 = bbox1[0][i] + coords1[i];
        float min2 = bbox2[0][i] + coords2[i], max2 = bbox2[0][i] + coords2[i];

        for (int j = 1; j < 8; ++j) {
            min1 = (bbox1[j][i] + coords1[i] < min1) ? bbox1[j][i] + coords1[i] : min1;
            max1 = (bbox1[j][i] + coords1[i] > max1) ? bbox1[j][i] + coords1[i] : max1;
            min2 = (bbox2[j][i] + coords2[i] < min2) ? bbox2[j][i] + coords2[i] : min2;
            max2 = (bbox2[j][i] + coords2[i] > max2) ? bbox2[j][i] + coords2[i] : max2;
        }

        if (!overlap(min1, max1, min2, max2))
            return false;

        float middle1 = (max1 + min1) / 2.0f;
        float middle2 = (max2 + min2) / 2.0f;
        collision[i] = (middle1 + middle2) / 2.0f;
    }
    return true;
}


static float distanceSquared(float x1, float y1, float z1, float x2, float y2, float z2) {
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dz = z2 - z1;
    return dx * dx + dy * dy + dz * dz;
}

static bool sphereBoxCollide(float sphereCenterX, float sphereCenterY, float sphereCenterZ, float sphereRadius,
                      VECTOR bbox[8], float coords[3], float collision[3]) {

    float closestX = fminf(fmaxf(sphereCenterX, bbox[0][0] + coords[0]), bbox[1][0] + coords[0]);
    float closestY = fminf(fmaxf(sphereCenterY, bbox[0][1] + coords[1]), bbox[2][1] + coords[1]);
    float closestZ = fminf(fmaxf(sphereCenterZ, bbox[0][2] + coords[2]), bbox[4][2] + coords[2]);


    float distanceSq = distanceSquared(sphereCenterX, sphereCenterY, sphereCenterZ, closestX, closestY, closestZ);
    bool collides = distanceSq <= (sphereRadius * sphereRadius);

    if (collides) {
  
        collision[0] = closestX;
        collision[1] = closestY;
        collision[2] = closestZ;
    }

    return collides;
}


static bool sphereSphereCollide(float sphere1X, float sphere1Y, float sphere1Z, float sphere1Radius,
                        float sphere2X, float sphere2Y, float sphere2Z, float sphere2Radius,
                        float collision[3]) {

    float dx = sphere2X - sphere1X;
    float dy = sphere2Y - sphere1Y;
    float dz = sphere2Z - sphere1Z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);


    if (distance <= (sphere1Radius + sphere2Radius)) {

        float scaleFactor = sphere1Radius / (sphere1Radius + sphere2Radius);
        collision[0] = sphere1X + dx * scaleFactor;
        collision[1] = sphere1Y + dy * scaleFactor;
        collision[2] = sphere1Z + dz * scaleFactor;
        return true; 
    }
    return false; 
}

static void createBox(VECTOR bbox[8], float size[3]) {
    float halfSize[3] = { size[0] / 2.0f, size[1] / 2.0f, size[2] / 2.0f };

    bbox[0][0] = -halfSize[0];
    bbox[0][1] = -halfSize[1];
    bbox[0][2] = -halfSize[2];

    bbox[1][0] = halfSize[0];
    bbox[1][1] = -halfSize[1];
    bbox[1][2] = -halfSize[2];

    bbox[2][0] = halfSize[0];
    bbox[2][1] = halfSize[1];
    bbox[2][2] = -halfSize[2];

    bbox[3][0] = -halfSize[0];
    bbox[3][1] = halfSize[1];
    bbox[3][2] = -halfSize[2];

    bbox[4][0] = -halfSize[0];
    bbox[4][1] = -halfSize[1];
    bbox[4][2] = halfSize[2];

    bbox[5][0] = halfSize[0];
    bbox[5][1] = -halfSize[1];
    bbox[5][2] = halfSize[2];

    bbox[6][0] = halfSize[0];
    bbox[6][1] = halfSize[1];
    bbox[6][2] = halfSize[2];

    bbox[7][0] = -halfSize[0];
    bbox[7][1] = halfSize[1];
    bbox[7][2] = halfSize[2];
}

static JSValue js_physics_bbox_collide(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    VECTOR bbox1[8], bbox2[8];
    float coords1[3], coords2[3], collision[3];

    JS_ToFloat32(ctx, &coords1[0], argv[1]);
    JS_ToFloat32(ctx, &coords1[1], argv[2]);
    JS_ToFloat32(ctx, &coords1[2], argv[3]);

    JS_ToFloat32(ctx, &coords2[0], argv[5]);
    JS_ToFloat32(ctx, &coords2[1], argv[6]);
    JS_ToFloat32(ctx, &coords2[2], argv[7]);

	for (int i = 0; i < 8; i++) {
		JSValue vertex1 = JS_GetPropertyUint32(ctx, argv[0], i);
        JSValue vertex2 = JS_GetPropertyUint32(ctx, argv[4], i);

		JS_ToFloat32(ctx, &bbox1[i][0], JS_GetPropertyStr(ctx, vertex1, "x"));
		JS_ToFloat32(ctx, &bbox1[i][1], JS_GetPropertyStr(ctx, vertex1, "y"));
		JS_ToFloat32(ctx, &bbox1[i][2], JS_GetPropertyStr(ctx, vertex1, "z"));
		bbox1[i][3] = 1.0f;

		JS_ToFloat32(ctx, &bbox2[i][0], JS_GetPropertyStr(ctx, vertex2, "x"));
		JS_ToFloat32(ctx, &bbox2[i][1], JS_GetPropertyStr(ctx, vertex2, "y"));
		JS_ToFloat32(ctx, &bbox2[i][2], JS_GetPropertyStr(ctx, vertex2, "z"));
		bbox2[i][3] = 1.0f;
	}

    if (boxBoxCollide(bbox1, bbox2, coords1, coords2, collision)) {
		JSValue obj = JS_NewObject(ctx);
	
		JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, collision[0]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, collision[1]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, collision[2]), JS_PROP_C_W_E);

        return obj;
    }

    return JS_UNDEFINED;
}

static JSValue js_physics_spherebox_collide(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    VECTOR bbox1[8];
    float coords1[3], sphereCoords[3], sphereRadius, collision[3];

	JS_ToFloat32(ctx, &sphereCoords[0], JS_GetPropertyStr(ctx, argv[0], "x"));
	JS_ToFloat32(ctx, &sphereCoords[1], JS_GetPropertyStr(ctx, argv[0], "y"));
	JS_ToFloat32(ctx, &sphereCoords[2], JS_GetPropertyStr(ctx, argv[0], "z"));
    JS_ToFloat32(ctx, &sphereRadius,    JS_GetPropertyStr(ctx, argv[0], "r"));

    JS_ToFloat32(ctx, &coords1[0], argv[2]);
    JS_ToFloat32(ctx, &coords1[1], argv[3]);
    JS_ToFloat32(ctx, &coords1[2], argv[4]);

	for (int i = 0; i < 8; i++) {
		JSValue vertex1 = JS_GetPropertyUint32(ctx, argv[1], i);

		JS_ToFloat32(ctx, &bbox1[i][0], JS_GetPropertyStr(ctx, vertex1, "x"));
		JS_ToFloat32(ctx, &bbox1[i][1], JS_GetPropertyStr(ctx, vertex1, "y"));
		JS_ToFloat32(ctx, &bbox1[i][2], JS_GetPropertyStr(ctx, vertex1, "z"));
		bbox1[i][3] = 1.0f;
	}

    if (sphereBoxCollide(sphereCoords[0], sphereCoords[1], sphereCoords[2], sphereRadius, bbox1, coords1, collision)) {
		JSValue obj = JS_NewObject(ctx);
	
		JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, collision[0]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, collision[1]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, collision[2]), JS_PROP_C_W_E);

        return obj;
    }

    return JS_UNDEFINED;
}

static JSValue js_physics_spheresphere_collide(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    float sphere1Coords[3], sphere1Radius, sphere2Coords[3], sphere2Radius, collision[3];

	JS_ToFloat32(ctx, &sphere1Coords[0], JS_GetPropertyStr(ctx, argv[0], "x"));
	JS_ToFloat32(ctx, &sphere1Coords[1], JS_GetPropertyStr(ctx, argv[0], "y"));
	JS_ToFloat32(ctx, &sphere1Coords[2], JS_GetPropertyStr(ctx, argv[0], "z"));
    JS_ToFloat32(ctx, &sphere1Radius,    JS_GetPropertyStr(ctx, argv[0], "r"));

	JS_ToFloat32(ctx, &sphere2Coords[0], JS_GetPropertyStr(ctx, argv[1], "x"));
	JS_ToFloat32(ctx, &sphere2Coords[1], JS_GetPropertyStr(ctx, argv[1], "y"));
	JS_ToFloat32(ctx, &sphere2Coords[2], JS_GetPropertyStr(ctx, argv[1], "z"));
    JS_ToFloat32(ctx, &sphere2Radius,    JS_GetPropertyStr(ctx, argv[1], "r"));

    if (sphereSphereCollide(sphere1Coords[0], sphere1Coords[1], sphere1Coords[2], sphere1Radius, 
                         sphere2Coords[0], sphere2Coords[1], sphere2Coords[2], sphere2Radius,  collision)) {

		JSValue obj = JS_NewObject(ctx);
	
		JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, collision[0]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, collision[1]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, collision[2]), JS_PROP_C_W_E);

        return obj;
    }

    return JS_UNDEFINED;
}


static JSValue js_physics_sphere_create(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    float coords1[3], sphereRadius;

    JS_ToFloat32(ctx, &coords1[0], argv[0]);
    JS_ToFloat32(ctx, &coords1[1], argv[1]);
    JS_ToFloat32(ctx, &coords1[2], argv[2]);
    JS_ToFloat32(ctx, &sphereRadius, argv[3]);

	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, coords1[0]),   JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, coords1[1]),   JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, coords1[2]),   JS_PROP_C_W_E);
    JS_DefinePropertyValueStr(ctx, obj, "r", JS_NewFloat32(ctx, sphereRadius), JS_PROP_C_W_E);

    return obj;
}

static JSValue js_physics_box_create(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    float size[3];
    VECTOR bbox[8];

    JS_ToFloat32(ctx, &size[0], argv[0]);
    JS_ToFloat32(ctx, &size[1], argv[1]);
    JS_ToFloat32(ctx, &size[2], argv[2]);

    createBox(bbox, size);

	JSValue array = JS_NewArray(ctx);

	for (int i = 0; i < 8; i++) {
		JSValue obj = JS_NewObject(ctx);
	
		JS_DefinePropertyValueStr(ctx, obj, "x", JS_NewFloat32(ctx, bbox[i][0]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "y", JS_NewFloat32(ctx, bbox[i][1]), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "z", JS_NewFloat32(ctx, bbox[i][2]), JS_PROP_C_W_E);
		JS_DefinePropertyValueUint32(ctx, array, i, obj, JS_PROP_C_W_E);
	}
	
	return array;
}

static const JSCFunctionListEntry js_physics_funcs[] = {
    JS_CFUNC_DEF("createBox",           3, js_physics_box_create),
    JS_CFUNC_DEF("createSphere",        4, js_physics_sphere_create),
    JS_CFUNC_DEF("boxBoxCollide",       8, js_physics_bbox_collide),
    JS_CFUNC_DEF("sphereBoxCollide",    5, js_physics_spherebox_collide),
    JS_CFUNC_DEF("sphereSphereCollide", 2, js_physics_spheresphere_collide),
};

static int js_physics_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, js_physics_funcs, countof(js_physics_funcs));
}

JSModuleDef *athena_physics_init(JSContext *ctx)
{
    return athena_push_module(ctx, js_physics_init, js_physics_funcs, countof(js_physics_funcs), "Physics");
}
