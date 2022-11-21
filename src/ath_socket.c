

#include "ath_env.h"
#include <netman.h>
#include <ps2ip.h>

static JSValue athena_socket_close(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    int s = get_obj_int(ctx, -1, "\xff""\xff""s");
	lwip_close(s);

	return 0;
}

static JSValue athena_socket_ctor(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "Socket takes only 2 arguments");
    if (!duk_is_constructor_call(ctx)) return DUK_RET_TYPE_ERROR;

    duk_push_this(ctx);

    int sin_family = duk_get_int(ctx, 0);

	int s = lwip_socket(sin_family, duk_get_int(ctx, 1), 0);

	duk_push_uint(ctx, s);
    duk_put_prop_string(ctx, -2, "\xff""\xff""s");

	duk_push_uint(ctx, sin_family);
    duk_put_prop_string(ctx, -2, "\xff""\xff""sin_family");

    return 0;
}

static JSValue athena_socket_connect(duk_context* ctx) {
    int argc = duk_get_top(ctx);
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "Socket.connect takes only 2 arguments");

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = get_obj_int(ctx, -1, "\xff""\xff""sin_family");
    addr.sin_addr.s_addr = inet_addr(duk_get_string(ctx, 0));
    addr.sin_port = PP_HTONS(duk_get_int(ctx, 1));

    int s = get_obj_int(ctx, -1, "\xff""\xff""s");

    int ret = lwip_connect(s, (struct sockaddr*)&addr, sizeof(addr));

    duk_push_int(ctx, ret);
    return 1;
}

static JSValue athena_socket_bind(duk_context* ctx) {
    int argc = duk_get_top(ctx);
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "Socket.bind takes only 2 arguments");

    struct sockaddr_in addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_len = sizeof(addr);
    addr.sin_family = get_obj_int(ctx, -1, "\xff""\xff""sin_family");
    addr.sin_addr.s_addr = inet_addr(duk_get_string(ctx, 0));
    addr.sin_port = PP_HTONS(duk_get_int(ctx, 1));

    int s = get_obj_int(ctx, -1, "\xff""\xff""s");

    int ret = lwip_bind(s, (struct sockaddr*)&addr, sizeof(addr));

    duk_push_int(ctx, ret);
    return 1;
}

static JSValue athena_socket_send(duk_context* ctx) {
    int argc = duk_get_top(ctx);
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "Socket.send takes a single argument");

    int s = get_obj_int(ctx, -1, "\xff""\xff""s");

    size_t len = 0;
    void* buf = duk_to_buffer(ctx, 0, &len);

    int ret = lwip_send(s, buf, len, MSG_DONTWAIT);

    duk_push_int(ctx, ret);
    return 1;
}

static JSValue athena_socket_listen(duk_context* ctx) {
    int argc = duk_get_top(ctx);
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "Socket.listen takes a single argument");

    int s = get_obj_int(ctx, -1, "\xff""\xff""s");

    int ret = lwip_listen(s, 0);

    duk_push_int(ctx, ret);
    return 1;
}

static JSValue athena_socket_recv(duk_context* ctx) {
    int argc = duk_get_top(ctx);
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "Socket.recv takes a single argument");

    int s = get_obj_int(ctx, -1, "\xff""\xff""s");

    int len = duk_get_int(ctx, 0);

    void* buf = duk_push_buffer(ctx, len, 0);

    lwip_recv(s, buf, len, MSG_PEEK);
    return 1;
}

void socket_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    duk_push_c_function(ctx, athena_socket_ctor, DUK_VARARGS);

    duk_push_object(ctx);

    duk_push_c_function(ctx, athena_socket_connect, 2);
    duk_put_prop_string(ctx, -2, "connect");

    duk_push_c_function(ctx, athena_socket_bind, 2);
    duk_put_prop_string(ctx, -2, "bind");

    duk_push_c_function(ctx, athena_socket_listen, 0);
    duk_put_prop_string(ctx, -2, "listen");

    duk_push_c_function(ctx, athena_socket_send, 1);
    duk_put_prop_string(ctx, -2, "send");

    duk_push_c_function(ctx, athena_socket_recv, 1);
    duk_put_prop_string(ctx, -2, "recv");

    duk_push_c_function(ctx, athena_socket_close, 0);
    duk_put_prop_string(ctx, -2, "close");

    duk_put_prop_string(ctx, -2, "prototype");

    duk_put_global_string(ctx, "Socket");

	duk_push_uint(ctx, AF_INET);
	duk_put_global_string(ctx, "AF_INET");

	duk_push_uint(ctx, SOCK_STREAM);
	duk_put_global_string(ctx, "SOCK_STREAM");

	duk_push_uint(ctx, SOCK_DGRAM);
	duk_put_global_string(ctx, "SOCK_DGRAM");

	duk_push_uint(ctx, SOCK_RAW);
	duk_put_global_string(ctx, "SOCK_RAW");
}