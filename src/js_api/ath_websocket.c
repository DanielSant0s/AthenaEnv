#include <ath_env.h>
#include <athena/websocket.h>
#include <macros.h>

#define WS_RECV_BUF_SIZE 16000

static JSClassID js_ws_class_id;

static JSValue js_ws_get(JSContext *ctx, JSValueConst this_val, int magic)
{
	if (magic == 0) {
		JSValue v = JS_GetPropertyStr(ctx, this_val, "_verifyTLS");
		if (JS_IsUndefined(v)) {
			JS_FreeValue(ctx, v);
			return JS_NewBool(ctx, 1);
		}
		return v;
	}
	return JS_UNDEFINED;
}

static JSValue js_ws_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
	(void)this_val;
	(void)val;
	if (magic == 0)
		return JS_ThrowTypeError(ctx, "verifyTLS is read-only for WebSocket instances");
	return JS_UNDEFINED;
}

static JSValue js_ws_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	const char *url = JS_ToCString(ctx, argv[0]);
	bool verify_tls = true;

	if (argc > 1 && JS_IsObject(argv[1])) {
		JSValue v = JS_GetPropertyStr(ctx, argv[1], "verifyTLS");
		if (!JS_IsUndefined(v)) {
			int b = JS_ToBool(ctx, v);
			if (b >= 0)
				verify_tls = b != 0;
		}
		JS_FreeValue(ctx, v);
	}

	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	if (JS_IsException(proto))
		return JS_EXCEPTION;
	obj = JS_NewObjectProtoClass(ctx, proto, js_ws_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj))
		return JS_EXCEPTION;

	AthenaWebSocket *ws = athena_websocket_connect(url, verify_tls);
	JS_FreeCString(ctx, url);

	if (!ws) {
		JS_FreeValue(ctx, obj);
		return JS_ThrowInternalError(ctx, "WebSocket native connect failed");
	}

	JS_SetOpaque(obj, ws);
	JS_DefinePropertyValueStr(ctx, obj, "_verifyTLS", JS_NewBool(ctx, verify_tls), JS_PROP_C_W_E);
	return obj;
}

static void js_ws_dtor(JSRuntime *rt, JSValue val)
{
	AthenaWebSocket *ws = JS_GetOpaque(val, js_ws_class_id);
	if (ws)
		athena_websocket_destroy(ws);
}

static JSValue js_ws_recv(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaWebSocket *ws = JS_GetOpaque2(ctx, this_val, js_ws_class_id);
	size_t rlen = 0;

	uint8_t *ws_buf = js_malloc(ctx, WS_RECV_BUF_SIZE);
	if (!ws_buf)
		return JS_EXCEPTION;

	JSValue ret = JS_UNDEFINED;
	if (athena_websocket_recv(ws, ws_buf, WS_RECV_BUF_SIZE, &rlen) == 0)
		ret = JS_NewArrayBufferCopy(ctx, ws_buf, rlen);

	js_free(ctx, ws_buf);
	return ret;
}

static JSValue js_ws_send(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaWebSocket *ws = JS_GetOpaque2(ctx, this_val, js_ws_class_id);
	uint8_t *buf;
	size_t size;

	buf = JS_GetArrayBuffer(ctx, &size, argv[0]);
	if (athena_websocket_send(ws, buf, size, true) != 0)
		return JS_ThrowInternalError(ctx, "WebSocket native send not implemented");

	return JS_NewUint32(ctx, size);
}

static JSValue js_ws_close(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaWebSocket *ws = JS_GetOpaque2(ctx, this_val, js_ws_class_id);
	if (!ws)
		return JS_UNDEFINED;

	athena_websocket_close(ws);
	JS_SetOpaque(this_val, NULL);
	return JS_UNDEFINED;
}

static JSClassDef js_ws_class = {
	"WebSocket",
	.finalizer = js_ws_dtor,
};

static const JSCFunctionListEntry athena_ws_funcs[] = {
	JS_CGETSET_MAGIC_DEF("verifyTLS", js_ws_get, js_ws_set, 0),
	JS_CFUNC_DEF("send", 1, js_ws_send),
	JS_CFUNC_DEF("recv", 0, js_ws_recv),
	JS_CFUNC_DEF("close", 0, js_ws_close),
};

JSValue athena_ws_create_class(JSContext *ctx)
{
	JSValue ws_proto, ws_class;

	if (!js_ws_class_id)
		JS_NewClassID(&js_ws_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_ws_class_id, &js_ws_class);

	ws_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, ws_proto, athena_ws_funcs, countof(athena_ws_funcs));

	ws_class = JS_NewCFunction2(ctx, js_ws_ctor, "WebSocket", 2, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, ws_class, ws_proto);
	JS_SetClassProto(ctx, js_ws_class_id, ws_proto);

	return ws_class;
}

static int ws_init(JSContext *ctx, JSModuleDef *m)
{
	JSValue ws_class = athena_ws_create_class(ctx);
	JS_SetModuleExport(ctx, m, "WebSocket", ws_class);
	return 0;
}

JSModuleDef *athena_ws_init(JSContext *ctx)
{
	JSModuleDef *m = JS_NewCModule(ctx, "WebSocket", ws_init);
	if (!m)
		return NULL;
	JS_AddModuleExport(ctx, m, "WebSocket");
	return m;
}
