#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <render.h>
#include <ath_env.h>

static JSClassID js_animation_collection_class_id;

typedef struct {
	athena_animation_collection collection;
} JSAnimCollection;

static void athena_animation_collection_dtor(JSRuntime *rt, JSValue val){
	JSAnimCollection* ro = JS_GetOpaque(val, js_animation_collection_class_id);

	if (!ro)
		return;

	if (ro->collection.animations)
		free(ro->collection.animations); 

	js_free_rt(rt, ro);

	JS_SetOpaque(val, NULL);
}

static JSValue athena_animation_collection_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
	JSValue obj = JS_UNDEFINED;
    JSValue proto;

    JSAnimCollection* ro = js_mallocz(ctx, sizeof(JSAnimCollection));
	
    if (!ro)
        return JS_EXCEPTION;

	const char *file_tbo = JS_ToCString(ctx, argv[0]); 

    load_gltf_animations(file_tbo, &ro->collection);

register_3d_animation_collection:
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    obj = JS_NewObjectProtoClass(ctx, proto, js_animation_collection_class_id);

    JS_FreeValue(ctx, proto);
    JS_SetOpaque(obj, ro);

    return obj;
}


static JSValue animation_collection_get_prop(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst receiver) {
    JSAnimCollection *m = JS_GetOpaque2(ctx, obj, js_animation_collection_class_id);
    
    const char *prop_name = JS_AtomToCString(ctx, prop);
    if (!prop_name) return JS_EXCEPTION;

    char *endptr;
    long index = strtol(prop_name, &endptr, 10);
    
    if (*endptr == '\0' && index >= 0) { 
        if (index < m->collection.count) {
            return JS_NewUint32(ctx, &m->collection.animations[index]);
        }
        
        return JS_EXCEPTION;
    } else {
        for (int i = 0; i < m->collection.count; i++) {
            if (!strcmp(m->collection.animations[i].name, prop_name)) {
                return JS_NewUint32(ctx, &m->collection.animations[i]);
            }
        }
    }
    
    JS_FreeCString(ctx, prop_name);

    return JS_UNDEFINED;  // Prop not found here, call the default GetProp
}

static int animation_collection_set_prop(JSContext *ctx, JSValueConst obj, JSAtom prop, JSValueConst val, JSValueConst receiver, int flags) {
    JSAnimCollection *m = JS_GetOpaque2(ctx, obj, js_animation_collection_class_id);
    
    const char *prop_name = JS_AtomToCString(ctx, prop);
    if (!prop_name) return -1;

    char *endptr;
    long index = strtol(prop_name, &endptr, 10);
    
    if (*endptr == '\0' && index >= 0) {
        JS_FreeCString(ctx, prop_name);
        uint32_t value;
        JS_ToUint32(ctx, &value, val);

        if (index < m->collection.count) {
            memcpy(&m->collection.animations[index], value, sizeof(athena_animation));

            return 1;
        }

        return 0;
    } else {
        uint32_t value;
        JS_ToUint32(ctx, &value, val);

        for (int i = 0; i < m->collection.count; i++) {
            if (!strcmp(m->collection.animations[i].name, prop_name)) {
                memcpy(&m->collection.animations[i], value, sizeof(athena_animation));

                JS_FreeCString(ctx, prop_name);

                return 1;
            }
        }
    }
    
    JS_FreeCString(ctx, prop_name);

    return 2; // Prop not found here, call the default SetProp
}


static JSClassExoticMethods em = {
    .get_property = animation_collection_get_prop,
    .set_property = animation_collection_set_prop
};

static JSClassDef js_animation_collection_class = {
    "AnimCollection",
    .finalizer = athena_animation_collection_dtor,
    .exotic = &em
}; 

static JSValue athena_rdfree(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	athena_animation_collection_dtor(JS_GetRuntime(ctx), this_val);

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry js_animation_collection_proto_funcs[] = {
	JS_CFUNC_DEF("free",  0,  athena_rdfree),
};

static int js_animation_collection_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue animation_collection_proto, animation_collection_class;
    
    /* create the Point class */
    JS_NewClassID(&js_animation_collection_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_animation_collection_class_id, &js_animation_collection_class);

    animation_collection_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, animation_collection_proto, js_animation_collection_proto_funcs, countof(js_animation_collection_proto_funcs));
    
    animation_collection_class = JS_NewCFunction2(ctx, athena_animation_collection_ctor, "AnimCollection", 1, JS_CFUNC_constructor, 0);

    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, animation_collection_class, animation_collection_proto);
    JS_SetClassProto(ctx, js_animation_collection_class_id, animation_collection_proto);
                      
    JS_SetModuleExport(ctx, m, "AnimCollection", animation_collection_class);
    return 0;
}


JSModuleDef *athena_anim_3d_init(JSContext* ctx){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "AnimCollection", js_animation_collection_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "AnimCollection");

	return m;
}