#include "include/network.h"
#include "include/taskman.h"
#include <time.h>

static JSClassID js_request_class_id;
static JSClassID js_ws_class_id;

static JSValue athena_nw_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0 && argc != 4) return JS_ThrowInternalError(ctx, "wrong number of arguments.");
	
    struct ip4_addr IP, NM, GW, DNS;

    NetManInit();

	if(ethApplyNetIFConfig(NETMAN_NETIF_ETH_LINK_MODE_AUTO) != 0) {
		return JS_ThrowInternalError(ctx, "Error: failed to set link mode.");
	}

    if (argc == 4){
        IP.addr = ipaddr_addr(JS_ToCString(ctx, argv[0]));
        NM.addr = ipaddr_addr(JS_ToCString(ctx, argv[1]));
        GW.addr = ipaddr_addr(JS_ToCString(ctx, argv[2]));
        DNS.addr = ipaddr_addr(JS_ToCString(ctx, argv[3]));
    } else {
        ip4_addr_set_zero(&IP);
	    ip4_addr_set_zero(&NM);
	    ip4_addr_set_zero(&GW);
	    ip4_addr_set_zero(&DNS);
    }

    ps2ipInit(&IP, &NM, &GW);
    ethApplyIPConfig((argc == 4? 0 : 1), &IP, &NM, &GW, &DNS);

    dbgprintf("Waiting for connection...\n");
    if(ethWaitValidNetIFLinkState() != 0) {
	    return JS_ThrowInternalError(ctx, "Error: failed to get valid link status.\n");
	}

    if(argc == 4) return JS_UNDEFINED;

    dbgprintf("Waiting for DHCP lease...\n");
	if (ethWaitValidDHCPState() != 0)
	{
		return JS_ThrowInternalError(ctx, "DHCP failed.\n");
	}

    dbgprintf("DHCP Connected.\n");

    curl_global_init(CURL_GLOBAL_ALL);
    curl = curl_easy_init();

    dbgprintf("cURL Started.\n");

	return JS_UNDEFINED;
}

static JSValue athena_nw_get_config(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	t_ip_info ip_info;
    JSValue obj;

	if (ps2ip_getconfig("sm0", &ip_info) >= 0)
	{
        obj = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, obj, "ip", JS_NewString(ctx, ip4addr_ntoa(&ip_info.ipaddr)), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "netmask", JS_NewString(ctx, ip4addr_ntoa(&ip_info.netmask)), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "gateway", JS_NewString(ctx, ip4addr_ntoa(&ip_info.gw)), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "dns", JS_NewString(ctx, ip4addr_ntoa(dns_getserver(0))), JS_PROP_C_W_E);
	} else {
		obj = JS_ThrowInternalError(ctx, "Unable to read network info.\n");
	}

    return obj;
}

static JSValue athena_nw_deinit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    curl_easy_cleanup(curl);
    curl_global_cleanup();
	ps2ipDeinit();
	NetManDeinit();

    return 0;
}

static JSValue athena_nw_gethostbyname(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    const char* host = JS_ToCString(ctx, argv[0]);
    struct hostent *host_address = gethostbyname(host);
    
    if (host_address == NULL)
        return JS_ThrowInternalError(ctx, "Unable to resolve address.\n");

    return JS_NewString(ctx, ip4addr_ntoa((struct in_addr*)host_address->h_addr));
}

static void athena_nw_dtor(JSRuntime *rt, JSValue val)
{
    JSRequestData *s = JS_GetOpaque(val, js_request_class_id);
    js_free_rt(rt, s);
}

static JSValue athena_nw_ctor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv){

    JSRequestData* req;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    req = js_mallocz(ctx, sizeof(*req));
    if (!req)
        return JS_EXCEPTION;

    req->keepalive = 0L;
    req->userpwd = NULL;
    req->useragent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36";
    req->headers_len = 0;
    req->postdata = NULL;

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
    js_free(ctx, req);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue athena_nw_get(JSContext *ctx, JSValueConst this_val, int magic)
{
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);

    if (!s)
        return JS_EXCEPTION;

    switch (magic)
    {
    case 0:
        return JS_NewBool(ctx, s->keepalive);
        break;

    case 1:
        return JS_NewString(ctx, s->useragent);
        break;

    case 2:
        return JS_NewString(ctx, s->userpwd);
        break;

    default:
        break;
    }

    return JS_UNDEFINED;
}



static JSValue athena_nw_set(JSContext *ctx, JSValueConst this_val, JSValue val, int magic)
{
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    long v;
    const char* str = NULL;

    if (!s)
        return JS_EXCEPTION;

    if (magic < 1) {
        JS_ToInt32(ctx, &v, val);
    } else {
        str = JS_ToCString(ctx, val);
    }

    switch (magic)
    {
    case 0:
        s->keepalive = v;
        break;

    case 1:
        s->useragent = str;
        break;
    
    case 2:
        s->userpwd = str;
        break;

    default:
        break;
    }

    return JS_UNDEFINED;
}

static JSValue athena_nw_get_headers(JSContext *ctx, JSValueConst this_val)
{
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);

    if (!s)
        return JS_EXCEPTION;

    JSValue arr = JS_NewArray(ctx);

    for (int i = 0; i < s->headers_len; i++) {
	    JS_DefinePropertyValueUint32(ctx, arr, i, JS_NewString(ctx, s->headers[i]), JS_PROP_C_W_E);
    }

    return arr;
}

static JSValue athena_nw_set_headers(JSContext *ctx, JSValueConst this_val, JSValue val)
{
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    int arr_len;

	if (!JS_IsArray(ctx, val)) {
	    return JS_ThrowTypeError(ctx, "You should use a string array.\n");
	}

	JSValue len = JS_GetPropertyStr(ctx, val, "length");
	JS_ToInt32(ctx, &s->headers_len, len);

    if (s->headers_len > 16) {
        return JS_ThrowRangeError(ctx, "16 headers is the maximum quantity.\n");
    }

	for (int i = 0; i < s->headers_len; i++) {
		len = JS_GetPropertyUint32(ctx, val, i);
	    s->headers[i] = (char*)JS_ToCString(ctx, len);
	} 

    return JS_UNDEFINED;
}

static JSValue athena_nw_requests_download(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;
        
    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->method = ATHENA_GET;
    s->save = true;

    s->chunk.timer = 0;
    s->chunk.transferring = false;
    s->chunk.fp = fopen(JS_ToCString(ctx, argv[1]), "wb");
    s->tid = -1;

    if(!s->chunk.fp) {
        return JS_ThrowInternalError(ctx, "asyncDownload: Error creating file\n");
    }

    requestThread(s);

    if (s->error) {
        return JS_ThrowInternalError(ctx, s->error);
    }

    return JS_UNDEFINED;
}

static JSValue athena_nw_requests_async_dl(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->method = ATHENA_GET;
    s->save = true;

    s->chunk.timer = 0;
    s->chunk.transferring = false;
    s->chunk.fp = fopen(JS_ToCString(ctx, argv[1]), "wb");

    if(!s->chunk.fp) {
        return JS_ThrowInternalError(ctx, "asyncDownload: Error creating file\n");
    }

    s->tid = create_task("Requests: Download", requestThread, 4096*10, 16);
    init_task(s->tid, (void*)s);

    return JS_UNDEFINED;

}

static JSValue athena_nw_requests_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->method = ATHENA_GET;
    s->save = false;

    s->chunk.timer = 0;
    s->chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    s->chunk.size = 0;    /* no data at this point */
    s->chunk.transferring = false;
    s->tid = -1;

    requestThread(s);

    if (s->error) {
        return JS_ThrowInternalError(ctx, s->error);
    }

    JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "text", JS_NewStringLen(ctx, s->chunk.memory, s->chunk.size), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "status_code", JS_NewInt32(ctx, s->response_code), JS_PROP_C_W_E);
	return obj;
}


static JSValue athena_nw_requests_async_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{

    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->method = ATHENA_GET;
    s->save = false;

    s->chunk.timer = 0;
    s->chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    s->chunk.size = 0;    /* no data at this point */
    s->chunk.transferring = false;

    s->tid = create_task("Requests: Get", requestThread, 1638400, 16);
    init_task(s->tid, (void*)s);

    return JS_UNDEFINED;

}

static JSValue athena_nw_requests_ready(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    int timeout = 999999999, transfer_timeout = 3000;

    if (argc > 0) {
        JS_ToInt32(ctx, &timeout, argv[0]);
        if(argc > 1) {
            JS_ToInt32(ctx, &transfer_timeout, argv[1]);
        }

        

        if ((clock() - s->chunk.timer) / 1000 > timeout && s->chunk.transferring) {
            s->ready = true;
        } else if ((clock() - s->chunk.timer) / 1000 > transfer_timeout && !s->chunk.transferring && s->chunk.timer > 0) {
            s->error = "Network: Asynchronous operation timeout.\n";
        } 
    }

    if(s->save && (s->ready || s->error)) {
        fclose(s->chunk.fp);
        s->url = NULL;
        s->chunk.memory = NULL;
        s->chunk.fp = NULL;
        s->chunk.size = 0;
        s->chunk.timer = 0;
        s->chunk.transferring = false;

        kill_task(s->tid);

        if(s->error) {
            return JS_ThrowInternalError(ctx, s->error);
        }
    }

    return JS_NewBool(ctx, s->ready);
}

static JSValue athena_nw_requests_getasyncdata(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    JSValue ret = JS_UNDEFINED;

    if (s->ready || s->error) {
        ret = JS_NewStringLen(ctx, s->chunk.memory, s->chunk.size);

        kill_task(s->tid);
        s->url = NULL;
        s->chunk.memory = NULL;
        s->chunk.fp = NULL;
        s->chunk.size = 0;
        s->chunk.timer = 0;
        s->chunk.transferring = false;
        s->postdata = NULL;

        if (s->error) {
            return JS_ThrowInternalError(ctx, s->error);
        }
    }

    return ret;
}

static JSValue athena_nw_requests_getasyncsize(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    return JS_NewUint32(ctx, s->chunk.size);
}


static JSValue athena_nw_requests_post(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;
    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->method = ATHENA_POST;
    s->save = false;

    s->chunk.timer = 0;
    s->chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    s->chunk.size = 0;    /* no data at this point */
    s->chunk.transferring = false;
    s->postdata = JS_ToCString(ctx, argv[1]);
    s->tid = -1;

    requestThread(s);

    if (s->error) {
        return JS_ThrowInternalError(ctx, s->error);
    }

    JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "text", JS_NewStringLen(ctx, s->chunk.memory, s->chunk.size), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "status_code", JS_NewInt32(ctx, s->response_code), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_nw_requests_async_post(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{

    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->postdata = JS_ToCString(ctx, argv[1]);
    s->method = ATHENA_POST;
    s->save = false;

    s->chunk.timer = 0;
    s->chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    s->chunk.size = 0;    /* no data at this point */
    s->chunk.transferring = false;

    s->tid = create_task("Requests: Post", requestThread, 4096*10, 16);
    init_task(s->tid, (void*)s);

    return JS_UNDEFINED;

}

static JSValue athena_nw_requests_head(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->method = ATHENA_HEAD;
    s->save = false;

    s->chunk.timer = 0;
    s->chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    s->chunk.size = 0;    /* no data at this point */
    s->chunk.transferring = false;
    s->tid = -1;

    requestThread(s);

    if (s->error) {
        return JS_ThrowInternalError(ctx, s->error);
    }

    JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "text", JS_NewStringLen(ctx, s->chunk.memory, s->chunk.size), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "status_code", JS_NewInt32(ctx, s->response_code), JS_PROP_C_W_E);
	return obj;
}


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

static JSClassDef js_request_class = {
    "Request",
    .finalizer = athena_nw_dtor,
}; 

static const JSCFunctionListEntry athena_request_funcs[] = {
    JS_CGETSET_MAGIC_DEF("keepalive",      athena_nw_get, athena_nw_set, 0),
    JS_CGETSET_MAGIC_DEF("useragent",      athena_nw_get, athena_nw_set, 1),
    JS_CGETSET_MAGIC_DEF("userpwd",        athena_nw_get, athena_nw_set, 2),
    JS_CGETSET_DEF("headers", athena_nw_get_headers, athena_nw_set_headers),

    JS_CFUNC_DEF("get", 1, athena_nw_requests_get),
    JS_CFUNC_DEF("head", 1, athena_nw_requests_head),
    JS_CFUNC_DEF("post", 2, athena_nw_requests_post),
    JS_CFUNC_DEF("download", 2, athena_nw_requests_download),

    JS_CFUNC_DEF("asyncGet", 1, athena_nw_requests_async_get),
    JS_CFUNC_DEF("asyncPost", 2, athena_nw_requests_async_post),
    JS_CFUNC_DEF("asyncDownload", 2, athena_nw_requests_async_dl),

    JS_CFUNC_DEF("getAsyncData", 0, athena_nw_requests_getasyncdata),
    JS_CFUNC_DEF("getAsyncSize", 0, athena_nw_requests_getasyncsize),
    JS_CFUNC_DEF("ready", 2, athena_nw_requests_ready),
};

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

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("init", 4, athena_nw_init),
    JS_CFUNC_DEF("getConfig", 0, athena_nw_get_config),
    JS_CFUNC_DEF("getHostbyName", 1, athena_nw_gethostbyname),
    JS_CFUNC_DEF("deinit", 0, athena_nw_deinit),
};


static int request_init(JSContext *ctx, JSModuleDef *m) {
    JSValue request_proto, request_class;
    
    /* create the Point class */
    JS_NewClassID(&js_request_class_id);
    JS_NewClass(JS_GetRuntime(ctx), js_request_class_id, &js_request_class);

    request_proto = JS_NewObject(ctx);
    JS_SetPropertyFunctionList(ctx, request_proto, athena_request_funcs, countof(athena_request_funcs));
    
    request_class = JS_NewCFunction2(ctx, athena_nw_ctor, "Request", 2, JS_CFUNC_constructor, 0);
    /* set proto.constructor and ctor.prototype */
    JS_SetConstructor(ctx, request_class, request_proto);
    JS_SetClassProto(ctx, js_request_class_id, request_proto);
                      
    JS_SetModuleExport(ctx, m, "Request", request_class);

    return 0;
}

JSModuleDef *athena_request_init(JSContext *ctx)
{
    JSModuleDef *m;
    m = JS_NewCModule(ctx, "Request", request_init);
    if (!m)
        return NULL;
    JS_AddModuleExport(ctx, m, "Request");
    return m;
}

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_network_init(JSContext* ctx){
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Network");
}
