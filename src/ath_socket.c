

#include "ath_env.h"
#include "include/network.h"
#include <malloc.h>

typedef struct {
    int id;
    int sin_family;
} JSSocketData;

static JSClassID js_socket_class_id;

static JSValue athena_socket_close(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    JSSocketData* s = JS_GetOpaque2(ctx, this_val, js_socket_class_id);
	lwip_close(s->id);

	return JS_UNDEFINED;
}

static JSValue athena_socket_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    JSSocketData* s;
	JSValue obj = JS_UNDEFINED;
    JSValue proto;
    int protocol;

    s = js_mallocz(ctx, sizeof(*s));
    if (!s)
        return JS_EXCEPTION;

    JS_ToInt32(ctx, &s->sin_family, argv[0]);
    JS_ToInt32(ctx, &protocol, argv[1]);

	s->id = socket(s->sin_family, protocol, 0);

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    obj = JS_NewObjectProtoClass(ctx, proto, js_socket_class_id);
    JS_FreeValue(ctx, proto);
    JS_SetOpaque(obj, s);

    return obj;
}

static JSValue athena_socket_connect(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "Socket.connect takes only 2 arguments");

    struct sockaddr_in addr;
    int32_t sin_port;

    JSSocketData* s = JS_GetOpaque2(ctx, this_val, js_socket_class_id);

    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = s->sin_family;
    addr.sin_addr.s_addr = inet_addr(JS_ToCString(ctx, argv[0]));
    JS_ToInt32(ctx, &sin_port, argv[1]);
    addr.sin_port = PP_HTONS(sin_port);

    int ret = connect(s->id, (struct sockaddr*)&addr, sizeof(addr));

    return JS_NewInt32(ctx, ret);
}

static JSValue athena_socket_bind(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "Socket.bind takes only 2 arguments");

    struct sockaddr_in addr;
    int32_t sin_port;

    JSSocketData* s = JS_GetOpaque2(ctx, this_val, js_socket_class_id);

    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = s->sin_family;
    addr.sin_addr.s_addr = inet_addr(JS_ToCString(ctx, argv[0]));
    JS_ToInt32(ctx, &sin_port, argv[1]);
    addr.sin_port = PP_HTONS(sin_port);

    int ret = bind(s->id, (struct sockaddr*)&addr, sizeof(addr));

    return JS_NewInt32(ctx, ret);
}

static JSValue athena_socket_send(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "Socket.send takes a single argument");

    JSSocketData* s = JS_GetOpaque2(ctx, this_val, js_socket_class_id);

    size_t len = 0;
    const char* buf = JS_ToCStringLen(ctx, &len, argv[0]);

    int ret = send(s->id, buf, len, MSG_DONTWAIT);

    return JS_NewInt32(ctx, ret);
}

static JSValue athena_socket_listen(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "Socket.listen takes a single argument");

    JSSocketData* s = JS_GetOpaque2(ctx, this_val, js_socket_class_id);

    int ret = listen(s->id, 0);

    return JS_NewInt32(ctx, ret);
}

static JSValue athena_socket_recv(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "Socket.recv takes a single argument");

    JSSocketData* s = JS_GetOpaque2(ctx, this_val, js_socket_class_id);

    int len;
    JS_ToInt32(ctx, &len, argv[0]);

    void* buf = js_mallocz(ctx, len);

    recv(s->id, buf, len, MSG_PEEK);
    return JS_NewStringLen(ctx, buf, len);
}

static JSClassDef js_socket_class = {
    "Socket",
    //.finalizer = js_s_finalizer,
}; 

static const JSCFunctionListEntry js_socket_proto_funcs[] = {
    JS_CFUNC_DEF("connect", 2, athena_socket_connect ),
    JS_CFUNC_DEF("bind", 2, athena_socket_bind ),
    JS_CFUNC_DEF("listen", 0, athena_socket_listen ),
    JS_CFUNC_DEF("send", 1, athena_socket_send ),
    JS_CFUNC_DEF("recv", 1, athena_socket_recv ),
    JS_CFUNC_DEF("close", 0, athena_socket_close ),
};

static int js_socket_init(JSContext *ctx, JSModuleDef *m)
{
    JSValue socket_proto, socket_class;
    
    /* create the Point class */
    JS_NewClassID(&js_socket_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_socket_class_id, &js_socket_class);

    socket_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, socket_proto, js_socket_proto_funcs, countof(js_socket_proto_funcs));
    
    socket_class = JS_NewCFunction2(ctx, athena_socket_ctor, "Socket", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, socket_class, socket_proto);
    JS_SetClassProto(ctx, js_socket_class_id, socket_proto);
                      
    JS_SetModuleExport(ctx, m, "Socket", socket_class);
    return 0;
}

static const JSCFunctionListEntry js_socket_consts[] = {
    JS_PROP_INT32_DEF("AF_INET", AF_INET, JS_PROP_CONFIGURABLE ),
    JS_PROP_INT32_DEF("SOCK_STREAM", SOCK_STREAM, JS_PROP_CONFIGURABLE ),
    JS_PROP_INT32_DEF("SOCK_DGRAM", SOCK_DGRAM, JS_PROP_CONFIGURABLE ),
    JS_PROP_INT32_DEF("SOCK_RAW", SOCK_RAW, JS_PROP_CONFIGURABLE ),
};


static int socket_consts_init(JSContext *ctx, JSModuleDef *m){
    return JS_SetModuleExportList(ctx, m, js_socket_consts, countof(js_socket_consts));
}

JSModuleDef *athena_socket_init(JSContext *ctx)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "Socket", js_socket_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Socket");

    athena_push_module(ctx, socket_consts_init, js_socket_consts, countof(js_socket_consts), "SocketConst");

    dbgprintf("AthenaEnv: %s module pushed at 0x%x\n", "Socket", m);
    return m;
}

