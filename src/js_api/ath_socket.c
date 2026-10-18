#include <ath_env.h>
#include <athena/socket.h>
#include <network.h>
#include <macros.h>

static JSClassID js_socket_class_id;

static JSValue js_socket_close(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaSocket *sock = JS_GetOpaque2(ctx, this_val, js_socket_class_id);
	if (!sock)
		return JS_UNDEFINED;

	athena_socket_destroy(sock);
	JS_SetOpaque(this_val, NULL);
	return JS_UNDEFINED;
}

static JSValue js_socket_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	JSValue obj = JS_UNDEFINED;
	JSValue proto;
	int sin_family, protocol;

	JS_ToInt32(ctx, &sin_family, argv[0]);
	JS_ToInt32(ctx, &protocol, argv[1]);

	AthenaSocket *sock = athena_socket_create(sin_family, protocol);
	if (!sock)
		return JS_EXCEPTION;

	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	obj = JS_NewObjectProtoClass(ctx, proto, js_socket_class_id);
	JS_FreeValue(ctx, proto);
	JS_SetOpaque(obj, sock);
	return obj;
}

static JSValue js_socket_connect(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "Socket.connect takes only 2 arguments");

	AthenaSocket *sock = JS_GetOpaque2(ctx, this_val, js_socket_class_id);
	const char *ip_str = JS_ToCString(ctx, argv[0]);
	int32_t sin_port;
	JS_ToInt32(ctx, &sin_port, argv[1]);

	int ret = athena_socket_connect(sock, ip_str, sin_port);
	JS_FreeCString(ctx, ip_str);
	return JS_NewInt32(ctx, ret);
}

static JSValue js_socket_bind(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 2)
		return JS_ThrowSyntaxError(ctx, "Socket.bind takes only 2 arguments");

	AthenaSocket *sock = JS_GetOpaque2(ctx, this_val, js_socket_class_id);
	const char *ip_str = JS_ToCString(ctx, argv[0]);
	int32_t sin_port;
	JS_ToInt32(ctx, &sin_port, argv[1]);

	int ret = athena_socket_bind(sock, ip_str, sin_port);
	JS_FreeCString(ctx, ip_str);
	return JS_NewInt32(ctx, ret);
}

static JSValue js_socket_send(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "Socket.send takes a single argument");

	AthenaSocket *sock = JS_GetOpaque2(ctx, this_val, js_socket_class_id);
	size_t len = 0;
	const char *buf = JS_ToCStringLen(ctx, &len, argv[0]);
	int ret = athena_socket_send(sock, buf, len);
	JS_FreeCString(ctx, buf);
	return JS_NewInt32(ctx, ret);
}

static JSValue js_socket_listen(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 0)
		return JS_ThrowSyntaxError(ctx, "Socket.listen takes a single argument");

	AthenaSocket *sock = JS_GetOpaque2(ctx, this_val, js_socket_class_id);
	return JS_NewInt32(ctx, athena_socket_listen(sock));
}

static JSValue js_socket_recv(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "Socket.recv takes a single argument");

	AthenaSocket *sock = JS_GetOpaque2(ctx, this_val, js_socket_class_id);
	int len;
	JS_ToInt32(ctx, &len, argv[0]);

	void *buf = js_mallocz(ctx, len);
	size_t received = 0;
	int ret = athena_socket_recv(sock, buf, len, &received);

	JSValue result = (ret > 0) ? JS_NewStringLen(ctx, buf, received) : JS_NewString(ctx, "");
	js_free(ctx, buf);
	return result;
}

static void js_socket_finalizer(JSRuntime *rt, JSValue val)
{
	AthenaSocket *sock = JS_GetOpaque(val, js_socket_class_id);
	if (sock)
		athena_socket_destroy(sock);
}

static JSClassDef js_socket_class = {
	"Socket",
	.finalizer = js_socket_finalizer,
};

static const JSCFunctionListEntry js_socket_proto_funcs[] = {
	JS_CFUNC_DEF("connect", 2, js_socket_connect),
	JS_CFUNC_DEF("bind", 2, js_socket_bind),
	JS_CFUNC_DEF("listen", 0, js_socket_listen),
	JS_CFUNC_DEF("send", 1, js_socket_send),
	JS_CFUNC_DEF("recv", 1, js_socket_recv),
	JS_CFUNC_DEF("close", 0, js_socket_close),
};

static const JSCFunctionListEntry js_socket_consts[] = {
	JS_PROP_INT32_DEF("AF_INET", AF_INET, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SOCK_STREAM", SOCK_STREAM, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SOCK_DGRAM", SOCK_DGRAM, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SOCK_RAW", SOCK_RAW, JS_PROP_CONFIGURABLE),
};

JSValue athena_socket_create_class(JSContext *ctx)
{
	JSValue socket_proto, socket_class;

	if (!js_socket_class_id)
		JS_NewClassID(&js_socket_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_socket_class_id, &js_socket_class);

	socket_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, socket_proto, js_socket_proto_funcs, countof(js_socket_proto_funcs));

	socket_class = JS_NewCFunction2(ctx, js_socket_ctor, "Socket", 2, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, socket_class, socket_proto);
	JS_SetClassProto(ctx, js_socket_class_id, socket_proto);
	JS_SetPropertyFunctionList(ctx, socket_class, js_socket_consts, countof(js_socket_consts));

	return socket_class;
}

static int js_socket_init(JSContext *ctx, JSModuleDef *m)
{
	JSValue socket_class = athena_socket_create_class(ctx);
	JS_SetModuleExport(ctx, m, "Socket", socket_class);
	return 0;
}

JSModuleDef *athena_socket_init(JSContext *ctx)
{
	JSModuleDef *m = JS_NewCModule(ctx, "Socket", js_socket_init);
	if (!m)
		return NULL;
	JS_AddModuleExport(ctx, m, "Socket");
	return m;
}
