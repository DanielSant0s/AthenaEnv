#include <network.h>
#include <ath_net.h>
#include <taskman.h>
#include <time.h>

static JSClassID js_ws_class_id;

static JSValue athena_ws_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    switch (magic) {
    case 0: { // verifyTLS
        JSValue v = JS_GetPropertyStr(ctx, this_val, "_verifyTLS");
        if (JS_IsUndefined(v)) {
            v = JS_NewBool(ctx, 1);
        }
        return v;
    }
    default:
        break;
    }
    return JS_UNDEFINED;
}

static JSValue athena_ws_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    (void)this_val; (void)val;
    switch (magic) {
    case 0: // verifyTLS
        return JS_ThrowTypeError(ctx, "verifyTLS is read-only for WebSocket instances");
    default:
        break;
    }
    return JS_UNDEFINED;
}

static JSValue athena_ws_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv){
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    // Native backend: create object with opaque WS context
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        return JS_EXCEPTION;
    obj = JS_NewObjectProtoClass(ctx, proto, js_ws_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        return JS_EXCEPTION;

    const char* url = JS_ToCString(ctx, argv[0]);

    bool verify_tls = true;
    if (argc > 1 && JS_IsObject(argv[1])) {
        JSValue v = JS_GetPropertyStr(ctx, argv[1], "verifyTLS");
        if (!JS_IsUndefined(v)) {
            int b = JS_ToBool(ctx, v);
            if (b >= 0) verify_tls = b != 0;
        }
        JS_FreeValue(ctx, v);
    }

    ath_ws_ctx_t *ws = ath_ws_connect(url, verify_tls);
    if (!ws) {
        JS_FreeValue(ctx, obj);
        return JS_ThrowInternalError(ctx, "WebSocket native connect failed");
    }
    JS_SetOpaque(obj, ws);
    JS_DefinePropertyValueStr(ctx, obj, "_verifyTLS", JS_NewBool(ctx, verify_tls), JS_PROP_C_W_E);
    return obj;
}

static void athena_ws_dtor(JSRuntime *rt, JSValue val)
{
    void *opaque = JS_GetOpaque(val, js_ws_class_id);
    ath_ws_ctx_t *ws = (ath_ws_ctx_t*)opaque;
    if (ws) ath_ws_close(ws);
    printf("Freeing WebSocket\n");
}

static uint8_t ws_buf[16000];

static JSValue athena_ws_recv(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    void *opaque = JS_GetOpaque2(ctx, this_val, js_ws_class_id);
    JSValue ret = JS_UNDEFINED;

    size_t rlen = 0;

    if (ath_ws_recv((ath_ws_ctx_t*)opaque, ws_buf, sizeof(ws_buf), &rlen) == 0) {
        ret = JS_NewArrayBufferCopy(ctx, ws_buf, rlen);
    }

    return ret;
}

static JSValue athena_ws_send(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    void *opaque = JS_GetOpaque2(ctx, this_val, js_ws_class_id);
    uint8_t *buf;
    size_t size;
    size_t sent = 0;

    buf = JS_GetArrayBuffer(ctx, &size, argv[0]);

    if (ath_ws_send((ath_ws_ctx_t*)opaque, buf, size, 1) != 0) {
        return JS_ThrowInternalError(ctx, "WebSocket native send not implemented");
    }
    sent = size;

    return JS_NewUint32(ctx, sent);
	
}

static JSClassDef js_ws_class = {
    "WebSocket",
    .finalizer = athena_ws_dtor,
}; 

static const JSCFunctionListEntry athena_ws_funcs[] = {
    JS_CGETSET_MAGIC_DEF("verifyTLS", athena_ws_get, athena_ws_set, 0),
    JS_CFUNC_DEF("send", 1, athena_ws_send),
    JS_CFUNC_DEF("recv", 0, athena_ws_recv),
};

static int ws_init(JSContext *ctx, JSModuleDef *m) {
    JSValue ws_proto, ws_class;
    
    /* create the Point class */
    JS_NewClassID(&js_ws_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_ws_class_id, &js_ws_class);

    ws_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, ws_proto, athena_ws_funcs, countof(athena_ws_funcs));
    
    ws_class = JS_NewCFunction2(ctx, athena_ws_ctor, "WebSocket", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, ws_class, ws_proto);
    JS_SetClassProto(ctx, js_ws_class_id, ws_proto);
                      
    JS_SetModuleExport(ctx, m, "WebSocket", ws_class);

    return 0;
}

JSModuleDef *athena_ws_init(JSContext *ctx)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "WebSocket", ws_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "WebSocket");
    return m;
}
