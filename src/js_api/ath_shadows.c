#include <ath_env.h>
#include <shadows.h>
#include <string.h>

static JSClassID js_shadow_projector_class_id;

typedef struct {
	ath_shadow_projector proj;
} JSShadowProjector;

static void js_shadow_projector_finalizer(JSRuntime *rt, JSValue val) {
	JSShadowProjector *s = JS_GetOpaque(val, js_shadow_projector_class_id);
	if (s) {
        shadow_projector_free(&s->proj);
		js_free_rt(rt, s);
	}
}

static JSValue js_shadow_projector_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSShadowProjector *s = js_mallocz(ctx, sizeof(*s));
	if (!s) return JS_EXCEPTION;

	JSImageData *img = NULL;
	if (argc > 0 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
		img = JS_GetOpaque2(ctx, argv[0], get_img_class_id());
		if (!img) { js_free(ctx, s); return JS_ThrowTypeError(ctx, "Shadows.Projector: expected Image as arg0"); }
	}

	shadow_projector_init(&s->proj, img? img->tex : NULL);

	JSValue proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	if (JS_IsException(proto)) { js_free(ctx, s); return JS_EXCEPTION; }
	JSValue obj = JS_NewObjectProtoClass(ctx, proto, js_shadow_projector_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj)) { js_free(ctx, s); return JS_EXCEPTION; }
	JS_SetOpaque(obj, s);
	return obj;
}

static JSValue js_shadow_set_transform(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	float m[16];
	for (int i = 0; i < 16; i++) {
		if (JS_ToFloat32(ctx, &m[i], JS_GetPropertyUint32(ctx, argv[0], i))) return JS_EXCEPTION;
	}
	shadow_projector_set_transform(&s->proj, m);
	return JS_UNDEFINED;
}

static JSValue js_shadow_set_size(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	float w, h; JS_ToFloat32(ctx, &w, argv[0]); JS_ToFloat32(ctx, &h, argv[1]);
	shadow_projector_set_size(&s->proj, w, h);
	return JS_UNDEFINED;
}

static JSValue js_shadow_set_grid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	int gx, gz; JS_ToInt32(ctx, &gx, argv[0]); JS_ToInt32(ctx, &gz, argv[1]);
	shadow_projector_set_grid(&s->proj, gx, gz);
	return JS_UNDEFINED;
}

static JSValue js_shadow_set_light_dir(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	float x, y, z; JS_ToFloat32(ctx, &x, argv[0]); JS_ToFloat32(ctx, &y, argv[1]); JS_ToFloat32(ctx, &z, argv[2]);
	shadow_projector_set_light_dir(&s->proj, x, y, z);
	return JS_UNDEFINED;
}

static JSValue js_shadow_set_bias(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	float b; JS_ToFloat32(ctx, &b, argv[0]);
	shadow_projector_set_bias(&s->proj, b);
	return JS_UNDEFINED;
}

static JSValue js_shadow_set_light_offset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
    if (!s) return JS_EXCEPTION;
    float d; JS_ToFloat32(ctx, &d, argv[0]);
    shadow_projector_set_light_offset(&s->proj, d);
    return JS_UNDEFINED;
}

static JSValue js_shadow_set_slope(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	float c; JS_ToFloat32(ctx, &c, argv[0]);
	shadow_projector_set_slope_limit(&s->proj, c);
	return JS_UNDEFINED;
}

static JSValue js_shadow_set_color(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
    if (!s) return JS_EXCEPTION;
    float r, g, b, a;
    JS_ToFloat32(ctx, &r, argv[0]); JS_ToFloat32(ctx, &g, argv[1]); JS_ToFloat32(ctx, &b, argv[2]); JS_ToFloat32(ctx, &a, argv[3]);
    shadow_projector_set_color(&s->proj, r, g, b, a);
    return JS_UNDEFINED;
}

static JSValue js_shadow_set_blend(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
    if (!s) return JS_EXCEPTION;
    int mode; JS_ToInt32(ctx, &mode, argv[0]);
    shadow_projector_set_blend(&s->proj, mode);
    return JS_UNDEFINED;
}

static JSValue js_shadow_set_uv_rect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
    if (!s) return JS_EXCEPTION;
    float u0, v0, u1, v1;
    JS_ToFloat32(ctx, &u0, argv[0]); JS_ToFloat32(ctx, &v0, argv[1]);
    JS_ToFloat32(ctx, &u1, argv[2]); JS_ToFloat32(ctx, &v1, argv[3]);
    shadow_projector_set_uv_rect(&s->proj, u0, v0, u1, v1);
    return JS_UNDEFINED;
}

static JSValue js_shadow_get(JSContext *ctx, JSValueConst this_val, int magic) {
    JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
    if (!s) return JS_EXCEPTION;
    JSValue obj = JS_NewObject(ctx);
    float *v = NULL;
    switch (magic) {
        case 0: v = s->proj.obj.position; break;
        case 1: v = s->proj.obj.rotation; break;
        case 2: v = s->proj.obj.scale;    break;
    }
    if (!v) return JS_EXCEPTION;
    JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat32(ctx, v[0]));
    JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat32(ctx, v[1]));
    JS_SetPropertyStr(ctx, obj, "z", JS_NewFloat32(ctx, v[2]));
    return obj;
}

static JSValue js_shadow_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic) {
    JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
    if (!s) return JS_EXCEPTION;
    switch (magic) {
        case 0:
            JS_ToFloat32(ctx, &s->proj.obj.position[0], JS_GetPropertyStr(ctx, val, "x"));
            JS_ToFloat32(ctx, &s->proj.obj.position[1], JS_GetPropertyStr(ctx, val, "y"));
            JS_ToFloat32(ctx, &s->proj.obj.position[2], JS_GetPropertyStr(ctx, val, "z"));
            break;
        case 1:
            JS_ToFloat32(ctx, &s->proj.obj.rotation[0], JS_GetPropertyStr(ctx, val, "x"));
            JS_ToFloat32(ctx, &s->proj.obj.rotation[1], JS_GetPropertyStr(ctx, val, "y"));
            JS_ToFloat32(ctx, &s->proj.obj.rotation[2], JS_GetPropertyStr(ctx, val, "z"));
            break;
        case 2:
            JS_ToFloat32(ctx, &s->proj.obj.scale[0], JS_GetPropertyStr(ctx, val, "x"));
            JS_ToFloat32(ctx, &s->proj.obj.scale[1], JS_GetPropertyStr(ctx, val, "y"));
            JS_ToFloat32(ctx, &s->proj.obj.scale[2], JS_GetPropertyStr(ctx, val, "z"));
            break;
    }
    create_transform_matrix(s->proj.transform, s->proj.obj.position, s->proj.obj.rotation, s->proj.obj.scale);
    return JS_UNDEFINED;
}

#ifdef ATHENA_ODE
static JSValue js_shadow_enable_raycast(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	JSSpace *space = JS_GetOpaque2(ctx, argv[0], js_space_class_id);
	int enable; JS_ToInt32(ctx, &enable, argv[1]);
	float length; JS_ToFloat32(ctx, &length, argv[2]);
	shadow_projector_enable_raycast(&s->proj, space? space->space : NULL, length, enable);
	return JS_UNDEFINED;
}
#endif

static JSValue js_shadow_render(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
	JSShadowProjector *s = JS_GetOpaque2(ctx, this_val, js_shadow_projector_class_id);
	if (!s) return JS_EXCEPTION;
	shadow_projector_render(&s->proj);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_shadow_projector_proto_funcs[] = {
	JS_CFUNC_DEF("setTransform", 1, js_shadow_set_transform),
	JS_CFUNC_DEF("setSize", 2, js_shadow_set_size),
	JS_CFUNC_DEF("setGrid", 2, js_shadow_set_grid),
	JS_CFUNC_DEF("setLightDir", 3, js_shadow_set_light_dir),
	JS_CFUNC_DEF("setBias", 1, js_shadow_set_bias),
    JS_CFUNC_DEF("setLightOffset", 1, js_shadow_set_light_offset),
	JS_CFUNC_DEF("setSlopeLimit", 1, js_shadow_set_slope),
    JS_CFUNC_DEF("setColor", 4, js_shadow_set_color),
    JS_CFUNC_DEF("setBlend", 1, js_shadow_set_blend),
    JS_CFUNC_DEF("setUVRect", 4, js_shadow_set_uv_rect),
#ifdef ATHENA_ODE
	JS_CFUNC_DEF("enableRaycast", 3, js_shadow_enable_raycast),
#endif
    JS_CGETSET_MAGIC_DEF("position",  js_shadow_get, js_shadow_set, 0),
    JS_CGETSET_MAGIC_DEF("rotation",  js_shadow_get, js_shadow_set, 1),
    JS_CGETSET_MAGIC_DEF("scale",     js_shadow_get, js_shadow_set, 2),
	JS_CFUNC_DEF("render", 0, js_shadow_render),
};

static int js_shadows_init(JSContext *ctx, JSModuleDef *m) {
	JSValue proto, cls;
	JS_NewClassID(&js_shadow_projector_class_id);
	JSClassDef def = { "ShadowProjector", .finalizer = js_shadow_projector_finalizer };
	JS_NewClass(JS_GetRuntime(ctx), js_shadow_projector_class_id, &def);
	proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, proto, js_shadow_projector_proto_funcs, countof(js_shadow_projector_proto_funcs));
	cls = JS_NewCFunction2(ctx, js_shadow_projector_ctor, "Projector", 1, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, cls, proto);
	JS_SetClassProto(ctx, js_shadow_projector_class_id, proto);
	JS_SetModuleExport(ctx, m, "Projector", cls);
    // Export blend mode constants
    static const JSCFunctionListEntry js_shadows_consts[] = {
        JS_PROP_INT32_DEF("SHADOW_BLEND_DARKEN", 0, JS_PROP_CONFIGURABLE),
        JS_PROP_INT32_DEF("SHADOW_BLEND_ALPHA", 1, JS_PROP_CONFIGURABLE),
        JS_PROP_INT32_DEF("SHADOW_BLEND_ADD", 2, JS_PROP_CONFIGURABLE),
    };
    JS_SetModuleExportList(ctx, m, js_shadows_consts, countof(js_shadows_consts));
	return 0;
}

JSModuleDef *athena_shadows_init(JSContext *ctx) {
	JSModuleDef *m = JS_NewCModule(ctx, "Shadows", js_shadows_init);
	if (!m) return NULL;
	JS_AddModuleExport(ctx, m, "Projector");
	return m;
}


