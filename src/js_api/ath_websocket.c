#include <network.h>
#include <taskman.h>
#include <time.h>

static JSClassID js_ws_class_id;

static JSValue athena_ws_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv){

    CURL* ws;
    CURLcode result;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    ws = curl_easy_init();
    if (!ws)
        return JS_EXCEPTION;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto))
        goto ws_fail;
    obj = JS_NewObjectProtoClass(ctx, proto, js_ws_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj))
        goto ws_fail;
    JS_SetOpaque(obj, ws);

    const char* url = JS_ToCString(ctx, argv[0]);
    curl_easy_setopt(ws, CURLOPT_URL, url);

    curl_easy_setopt(ws, CURLOPT_CONNECT_ONLY, 2L);

    /* only do the initial bootstrap */
    //curl_easy_setopt(ws, CURLOPT_WS_OPTIONS, CURLWS_ALONE);

    curl_easy_setopt(ws, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(ws, CURLOPT_SSL_VERIFYHOST, 0L);

    result = curl_easy_perform(ws);

    if(CURLE_OK != result) {
        curl_easy_cleanup(ws);
        JS_FreeValue(ctx, obj);
        return JS_ThrowInternalError(ctx, "Error %d while connecting to WebSocket", result);
    }

    return obj;

 ws_fail:
    curl_easy_cleanup(ws);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static void athena_ws_dtor(JSRuntime *rt, JSValue val)
{
    CURL *ws = JS_GetOpaque(val, js_ws_class_id);
    curl_easy_cleanup(ws);
    printf("Freeing WebSocket\n");
}

static uint8_t ws_buf[16000];

static JSValue athena_ws_recv(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    CURL *ws = JS_GetOpaque2(ctx, this_val, js_ws_class_id);
    JSValue ret = JS_UNDEFINED;

    size_t rlen = 0;
    struct curl_ws_frame *meta;

    if(curl_ws_recv(ws, ws_buf, sizeof(ws_buf), &rlen, &meta) == CURLE_OK) {
        ret = JS_NewArrayBufferCopy(ctx, ws_buf, rlen);
    }

    return ret;
}

static JSValue athena_ws_send(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    CURL *ws = JS_GetOpaque2(ctx, this_val, js_ws_class_id);
    uint8_t *buf;
    size_t size;
    size_t sent = 0;
    struct curl_ws_frame *meta;

    buf = JS_GetArrayBuffer(ctx, &size, argv[0]);

    if(curl_ws_send(ws, buf, size, &sent, 0, CURLWS_BINARY) != CURLE_OK) {
        return JS_ThrowInternalError(ctx, "Error on WebSocket send");
    }

    return JS_NewUint32(ctx, sent);
	
}

static JSClassDef js_ws_class = {
    "WebSocket",
    .finalizer = athena_ws_dtor,
}; 

static const JSCFunctionListEntry athena_ws_funcs[] = {
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
