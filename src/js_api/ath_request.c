#include <ath_env.h>
#include <athena/request.h>
#include <macros.h>

static JSClassID js_request_class_id;

static void js_request_dtor(JSRuntime *rt, JSValue val)
{
	AthenaRequest *req = JS_GetOpaque(val, js_request_class_id);
	if (req)
		athena_request_destroy(req);
}

static JSValue js_request_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv)
{
	AthenaRequest *req = athena_request_create();
	JSValue obj = JS_UNDEFINED;
	JSValue proto;

	if (!req)
		return JS_EXCEPTION;

	proto = JS_GetPropertyStr(ctx, new_target, "prototype");
	if (JS_IsException(proto))
		goto fail;
	obj = JS_NewObjectProtoClass(ctx, proto, js_request_class_id);
	JS_FreeValue(ctx, proto);
	if (JS_IsException(obj))
		goto fail;
	JS_SetOpaque(obj, req);
	return obj;

 fail:
	athena_request_destroy(req);
	JS_FreeValue(ctx, obj);
	return JS_EXCEPTION;
}

static JSValue js_request_get_prop(JSContext *ctx, JSValueConst this_val, int magic)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	if (!req)
		return JS_EXCEPTION;

	switch (magic) {
	case 0: return JS_NewBool(ctx, req->keepalive);
	case 1: return JS_NewString(ctx, req->useragent);
	case 2: return JS_NewString(ctx, req->userpwd);
	case 3: return JS_NewInt32(ctx, req->timeout_ms);
	case 4: return JS_NewBool(ctx, req->verify_tls);
	case 5: return JS_NewBool(ctx, req->follow_redirects);
	default: break;
	}
	return JS_UNDEFINED;
}

static JSValue js_request_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	long v;
	const char *str = NULL;

	if (!req)
		return JS_EXCEPTION;

	if (magic == 0 || magic == 3 || magic == 4 || magic == 5)
		JS_ToInt32(ctx, &v, val);
	else
		str = JS_ToCString(ctx, val);

	switch (magic) {
	case 0: athena_request_set_keepalive(req, v != 0); break;
	case 1: athena_request_set_useragent(req, str); break;
	case 2: athena_request_set_userpwd(req, str); break;
	case 3: athena_request_set_timeout(req, (int)v); break;
	case 4: athena_request_set_verify_tls(req, v != 0); break;
	case 5: athena_request_set_follow_redirects(req, v != 0); break;
	default: break;
	}
	return JS_UNDEFINED;
}

static JSValue js_request_get_headers(JSContext *ctx, JSValueConst this_val)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	if (!req)
		return JS_EXCEPTION;

	JSValue arr = JS_NewArray(ctx);
	for (int i = 0; i < req->headers_len; i++)
		JS_DefinePropertyValueUint32(ctx, arr, i, JS_NewString(ctx, req->headers[i]), JS_PROP_C_W_E);
	return arr;
}

static JSValue js_request_set_headers(JSContext *ctx, JSValueConst this_val, JSValue val)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	int arr_len;

	if (!JS_IsArray(ctx, val))
		return JS_ThrowTypeError(ctx, "You should use a string array.\n");

	JSValue len = JS_GetPropertyStr(ctx, val, "length");
	JS_ToInt32(ctx, &arr_len, len);
	JS_FreeValue(ctx, len);

	if (arr_len > 16)
		return JS_ThrowRangeError(ctx, "16 headers is the maximum quantity.\n");

	const char *headers[16];
	for (int i = 0; i < arr_len; i++) {
		JSValue item = JS_GetPropertyUint32(ctx, val, i);
		headers[i] = JS_ToCString(ctx, item);
		JS_FreeValue(ctx, item);
	}

	athena_request_set_headers(req, headers, arr_len);
	return JS_UNDEFINED;
}

static JSValue js_request_make_response(JSContext *ctx, AthenaRequestResponse *resp)
{
	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "text", JS_NewStringLen(ctx, resp->body, resp->body_size), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "status_code", JS_NewInt32(ctx, (int32_t)resp->status_code), JS_PROP_C_W_E);
	if (resp->headers)
		JS_DefinePropertyValueStr(ctx, obj, "headers", JS_NewString(ctx, resp->headers), JS_PROP_C_W_E);
	athena_request_response_free(resp);
	return obj;
}

static JSValue js_request_download(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	const char *url = JS_ToCString(ctx, argv[0]);
	const char *path = JS_ToCString(ctx, argv[1]);

	if (athena_request_download(req, url, path) != 0) {
		JS_FreeCString(ctx, url);
		JS_FreeCString(ctx, path);
		return JS_ThrowInternalError(ctx, req->error ? req->error : "asyncDownload: Error creating file\n");
	}

	JS_FreeCString(ctx, url);
	JS_FreeCString(ctx, path);
	return JS_UNDEFINED;
}

static JSValue js_request_async_dl(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	const char *path;

	req->url = JS_ToCString(ctx, argv[0]);
	path = JS_ToCString(ctx, argv[1]);
	req->method = ATHENA_REQUEST_GET;
	req->save = true;
	req->chunk.timer = 0;
	req->chunk.transferring = false;
	req->chunk.size = 0;
	req->chunk.fp = path ? fopen(path, "wb") : NULL;
	JS_FreeCString(ctx, path);

	if (!req->chunk.fp)
		return JS_ThrowInternalError(ctx, "asyncDownload: Error creating file\n");

	if (athena_request_run_async(req, "Requests: Download") != 0) {
		fclose(req->chunk.fp);
		req->chunk.fp = NULL;
		return JS_ThrowInternalError(ctx, "asyncDownload: unable to create task\n");
	}
	return JS_UNDEFINED;
}

static JSValue js_request_http_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	const char *url = JS_ToCString(ctx, argv[0]);
	AthenaRequestResponse *resp = NULL;

	if (athena_request_get(req, url, &resp) != 0) {
		JS_FreeCString(ctx, url);
		return JS_ThrowInternalError(ctx, req->error ? req->error : "Network error");
	}

	JS_FreeCString(ctx, url);
	return js_request_make_response(ctx, resp);
}

static JSValue js_request_async_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	req->url = JS_ToCString(ctx, argv[0]);
	req->method = ATHENA_REQUEST_GET;
	req->save = false;
	req->chunk.memory = malloc(1);
	req->chunk.size = 0;

	if (athena_request_run_async(req, "Requests: Get") != 0) {
		free(req->chunk.memory);
		req->chunk.memory = NULL;
		return JS_ThrowInternalError(ctx, "asyncGet: unable to create task\n");
	}
	return JS_UNDEFINED;
}

static JSValue js_request_ready(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	int timeout = 999999999;
	int transfer_timeout = 3000;

	if (argc > 0) {
		JS_ToInt32(ctx, &timeout, argv[0]);
		if (argc > 1)
			JS_ToInt32(ctx, &transfer_timeout, argv[1]);
	}

	bool ready = athena_request_poll(req, timeout, transfer_timeout);
	if (req->error)
		return JS_ThrowInternalError(ctx, req->error);

	return JS_NewBool(ctx, ready);
}

static JSValue js_request_getasyncdata(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	AthenaRequestResponse *resp = athena_request_take_response(req);

	if (req->error)
		return JS_ThrowInternalError(ctx, req->error);
	if (!resp)
		return JS_UNDEFINED;

	JSValue ret = JS_NewStringLen(ctx, resp->body, resp->body_size);
	athena_request_response_free(resp);
	return ret;
}

static JSValue js_request_getasyncsize(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	return JS_NewUint32(ctx, req->chunk.size);
}

static JSValue js_request_post(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	const char *url = JS_ToCString(ctx, argv[0]);
	const char *body = JS_ToCString(ctx, argv[1]);
	AthenaRequestResponse *resp = NULL;

	if (athena_request_post(req, url, body, &resp) != 0) {
		JS_FreeCString(ctx, url);
		JS_FreeCString(ctx, body);
		return JS_ThrowInternalError(ctx, req->error ? req->error : "Network error");
	}

	JS_FreeCString(ctx, url);
	JS_FreeCString(ctx, body);
	return js_request_make_response(ctx, resp);
}

static JSValue js_request_async_post(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	req->url = JS_ToCString(ctx, argv[0]);
	req->postdata = JS_ToCString(ctx, argv[1]);
	req->method = ATHENA_REQUEST_POST;
	req->save = false;
	req->chunk.memory = malloc(1);
	req->chunk.size = 0;

	if (athena_request_run_async(req, "Requests: Post") != 0) {
		free(req->chunk.memory);
		req->chunk.memory = NULL;
		return JS_ThrowInternalError(ctx, "asyncPost: unable to create task\n");
	}
	return JS_UNDEFINED;
}

static JSValue js_request_head(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaRequest *req = JS_GetOpaque2(ctx, this_val, js_request_class_id);
	const char *url = JS_ToCString(ctx, argv[0]);
	AthenaRequestResponse *resp = NULL;

	if (athena_request_head(req, url, &resp) != 0) {
		JS_FreeCString(ctx, url);
		return JS_ThrowInternalError(ctx, req->error ? req->error : "Network error");
	}

	JS_FreeCString(ctx, url);
	return js_request_make_response(ctx, resp);
}

static JSClassDef js_request_class = {
	"Request",
	.finalizer = js_request_dtor,
};

static const JSCFunctionListEntry js_request_funcs[] = {
	JS_CGETSET_MAGIC_DEF("keepalive", js_request_get_prop, js_request_set, 0),
	JS_CGETSET_MAGIC_DEF("useragent", js_request_get_prop, js_request_set, 1),
	JS_CGETSET_MAGIC_DEF("userpwd", js_request_get_prop, js_request_set, 2),
	JS_CGETSET_MAGIC_DEF("timeout", js_request_get_prop, js_request_set, 3),
	JS_CGETSET_MAGIC_DEF("verifyTLS", js_request_get_prop, js_request_set, 4),
	JS_CGETSET_MAGIC_DEF("followRedirects", js_request_get_prop, js_request_set, 5),
	JS_CGETSET_DEF("headers", js_request_get_headers, js_request_set_headers),
	JS_CFUNC_DEF("get", 1, js_request_http_get),
	JS_CFUNC_DEF("head", 1, js_request_head),
	JS_CFUNC_DEF("post", 2, js_request_post),
	JS_CFUNC_DEF("download", 2, js_request_download),
	JS_CFUNC_DEF("asyncGet", 1, js_request_async_get),
	JS_CFUNC_DEF("asyncPost", 2, js_request_async_post),
	JS_CFUNC_DEF("asyncDownload", 2, js_request_async_dl),
	JS_CFUNC_DEF("getAsyncData", 0, js_request_getasyncdata),
	JS_CFUNC_DEF("getAsyncSize", 0, js_request_getasyncsize),
	JS_CFUNC_DEF("ready", 2, js_request_ready),
};

JSValue athena_request_create_class(JSContext *ctx)
{
	JSValue request_proto, request_class;

	if (!js_request_class_id)
		JS_NewClassID(&js_request_class_id);
	JS_NewClass(JS_GetRuntime(ctx), js_request_class_id, &js_request_class);

	request_proto = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, request_proto, js_request_funcs, countof(js_request_funcs));

	request_class = JS_NewCFunction2(ctx, js_request_ctor, "Request", 0, JS_CFUNC_constructor, 0);
	JS_SetConstructor(ctx, request_class, request_proto);
	JS_SetClassProto(ctx, js_request_class_id, request_proto);

	return request_class;
}

static int request_init(JSContext *ctx, JSModuleDef *m)
{
	JSValue request_class = athena_request_create_class(ctx);
	JS_SetModuleExport(ctx, m, "Request", request_class);
	return 0;
}

JSModuleDef *athena_request_init(JSContext *ctx)
{
	JSModuleDef *m = JS_NewCModule(ctx, "Request", request_init);
	if (!m)
		return NULL;
	JS_AddModuleExport(ctx, m, "Request");
	return m;
}
