#include "include/network.h"
#include "include/taskman.h"
#include <time.h>

static JSClassID js_request_class_id;

static CURL* curl = NULL;

typedef struct
{
    bool ready;
    int tid;
    const char* url;
    const char* fname;
    const char* error;
    long keepalive;
    const char* userpwd;
    const char* useragent;
    struct MemoryStruct chunk;
    long response_code;
    char* headers[16];
    int headers_len;
} JSRequestData;

static void async_download(void* data) {
    CURLcode res;

    JSRequestData *s = data;

    curl_easy_reset(curl);

    s->chunk.fp = fopen(s->fname, "wb");
    if (s->chunk.fp) {
        struct curl_slist *header_chunk = NULL;

        curl_easy_setopt(curl, CURLOPT_URL, s->url);

        for(int i = 0; i < s->headers_len; i++) {
            header_chunk = curl_slist_append(header_chunk, s->headers[i]);
        }

	    header_chunk = curl_slist_append(header_chunk, "Accept: */*");
	    header_chunk = curl_slist_append(header_chunk, "Content-Type: application/json");
	    header_chunk = curl_slist_append(header_chunk, "Content-Length: 0");

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);

        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AsyncWriteFileCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&(s->chunk));

        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_3);

        curl_easy_setopt(curl, CURLOPT_USERPWD, s->userpwd);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, s->useragent);

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, s->keepalive);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        s->chunk.timer = clock();
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            s->error = "Error while downloading file\n"; //, curl_easy_strerror(res));
        }

        fclose(s->chunk.fp);
    } else {
        s->error = "Error while creating file\n";
    }

    s->ready = true;

    //exitkill_task();
}

static void async_get(void* data)
{
    CURLcode res;

    JSRequestData* req = data;

    req->chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    req->chunk.size = 0;    /* no data at this point */

    curl_easy_reset(curl);

    struct curl_slist *header_chunk = NULL;
    
    for(int i = 0; i < req->headers_len; i++) {
        header_chunk = curl_slist_append(header_chunk, req->headers[i]);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);

    curl_easy_setopt(curl, CURLOPT_URL, req->url);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    curl_easy_setopt(curl, CURLOPT_USERPWD, req->userpwd);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, req->useragent);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, req->keepalive);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AsyncWriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&(req->chunk));

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &(req->response_code));
 
    /* Perform the request, res will get the return code */
    req->chunk.timer = clock();
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK) {
        req->error = "curl_easy_perform() failed: %s\n"; //, curl_easy_strerror(res);
    }

    req->ready = true;

    //exitkill_task();
}

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
        JS_DefinePropertyValueStr(ctx, obj, "ip", JS_NewString(ctx, inet_ntoa(ip_info.ipaddr)), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "netmask", JS_NewString(ctx, inet_ntoa(ip_info.netmask)), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "gateway", JS_NewString(ctx, inet_ntoa(ip_info.gw)), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "dns", JS_NewString(ctx, inet_ntoa(*dns_getserver(0))), JS_PROP_C_W_E);
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
    struct hostent *host_address = lwip_gethostbyname(host);
    
    if (host_address == NULL)
        return JS_ThrowInternalError(ctx, "Unable to resolve address.\n");

    return JS_NewString(ctx, inet_ntoa(*(struct in_addr*)host_address->h_addr));
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

    FILE *fp;
    CURLcode res;

    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;
        
    const char* url = JS_ToCString(ctx, argv[0]);
    const char* filename = JS_ToCString(ctx, argv[1]);

    curl_easy_reset(curl);

    fp = fopen(filename, "wb");
    if (fp) {
        struct curl_slist *header_chunk = NULL;

        for(int i = 0; i < s->headers_len; i++) {
            header_chunk = curl_slist_append(header_chunk, s->headers[i]);
        }

        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);

        curl_easy_setopt(curl, CURLOPT_USERPWD, s->userpwd);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, s->useragent);

        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, s->keepalive);

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            return JS_ThrowInternalError(ctx, "Error while downloading file: %s\n", curl_easy_strerror(res));
        }

        fclose(fp);
    } else {
        return JS_ThrowInternalError(ctx, "Error while creating file: %s\n", filename);
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
    s->fname = JS_ToCString(ctx, argv[1]);
    s->chunk.timer = 0;

    s->chunk.transferring = false;

    s->tid = create_task("Requests: Asynchronous download", async_download, 4096*10, 16);
    init_task(s->tid, (void*)s);

    return JS_UNDEFINED;

}

static JSValue athena_nw_requests_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{

    CURLcode res;
    long response_code;
    double elapsed;
    char* url;

    struct MemoryStruct chunk;

    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    struct curl_slist *header_chunk = NULL;

    curl_easy_reset(curl);
    
    for(int i = 0; i < s->headers_len; i++) {
        header_chunk = curl_slist_append(header_chunk, s->headers[i]);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);

    curl_easy_setopt(curl, CURLOPT_URL, JS_ToCString(ctx, argv[0]));

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    curl_easy_setopt(curl, CURLOPT_USERPWD, s->userpwd);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, s->useragent);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, s->keepalive);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
 
    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK) {
        return JS_ThrowInternalError(ctx, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
    }

    JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "text", JS_NewStringLen(ctx, chunk.memory, chunk.size), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "status_code", JS_NewInt32(ctx, (int)response_code), JS_PROP_C_W_E);
	return obj;
}


static JSValue athena_nw_requests_async_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    pthread_t tid;
    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    s->error = NULL;
    s->ready = false;
    s->url = JS_ToCString(ctx, argv[0]);
    s->chunk.timer = 0;
    s->fname = NULL;

    s->chunk.transferring = false;

    s->tid = create_task("Requests: Asynchronous get", async_get, 4096*16, 16);
    init_task(s->tid, (void*)s);

    return JS_UNDEFINED;

}

static JSValue athena_nw_requests_ready(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    JSRequestData* s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    int timeout = 999999999, transfer_timeout = 60;

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

    if(s->fname != NULL && (s->ready || s->error)) {
        fclose(s->chunk.fp);
        s->fname = NULL;
        s->url = NULL;
        s->chunk.memory = NULL;
        s->chunk.fp = NULL;
        s->chunk.size = 0;
        s->chunk.timer = 0;
        s->chunk.transferring = false;

        kill_task(s->tid);

        //curl = NULL;
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

       // kill_task(s->tid);
        s->fname = NULL;
        s->url = NULL;
        s->chunk.memory = NULL;
        s->chunk.fp = NULL;
        s->chunk.size = 0;
        s->chunk.timer = 0;
        s->chunk.transferring = false;

        
    

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
    CURLcode res;
    long response_code;

    struct MemoryStruct chunk;

    JSRequestData *s = JS_GetOpaque2(ctx, this_val, js_request_class_id);
    if (!s)
        return JS_EXCEPTION;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    unsigned int len = 0;

    struct curl_slist *header_chunk = NULL;

    curl_easy_reset(curl);
    
    for(int i = 0; i < s->headers_len; i++) {
        header_chunk = curl_slist_append(header_chunk, s->headers[i]);
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);

    curl_easy_setopt(curl, CURLOPT_URL, JS_ToCString(ctx, argv[0]));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, JS_ToCStringLen(ctx, &len, argv[1]));
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)len);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    curl_easy_setopt(curl, CURLOPT_USERPWD, s->userpwd);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, s->useragent);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, s->keepalive);

    /* send all data to this function  */
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    /* Perform the request, res will get the return code */
    res = curl_easy_perform(curl);
    /* Check for errors */
    if(res != CURLE_OK)
        return JS_ThrowInternalError(ctx, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

    JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "text", JS_NewStringLen(ctx, chunk.memory, chunk.size), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "status_code", JS_NewInt32(ctx, (int)response_code), JS_PROP_C_W_E);

	return obj;
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
    JS_CFUNC_DEF("asyncGet", 1, athena_nw_requests_async_get),
    JS_CFUNC_DEF("asyncDownload", 2, athena_nw_requests_async_dl),
    JS_CFUNC_DEF("getAsyncData", 0, athena_nw_requests_getasyncdata),
    JS_CFUNC_DEF("getAsyncSize", 0, athena_nw_requests_getasyncsize),
    JS_CFUNC_DEF("ready", 2, athena_nw_requests_ready),
    JS_CFUNC_DEF("post", 2, athena_nw_requests_post),
    JS_CFUNC_DEF("download", 2, athena_nw_requests_download),
};

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
