#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ath_lua.h"
#include <network.h>
#include <athena/net.h>
#include <athena/request.h>
#include <athena/socket.h>
#include <athena/websocket.h>

/*
 * Network, Request, Socket and WebSocket. Built into athena_lua.elf with
 * STATIC_NETWORK=1, or as athena_network_lua.erl (the default), which the Lua
 * runtime loads on first use of any of these globals.
 *
 * The request facade keeps pointers to the strings it is given (url, user
 * agent, headers...), so the binding keeps those Lua strings referenced from
 * the Request object for as long as a transfer may read them.
 *
 * Async transfers: req:asyncGet(url) then `await(req)` inside async() --
 * req:ready() is what await polls -- and req:getAsyncData().
 */

#define REQUEST_CLASS "Request"
#define SOCKET_CLASS "Socket"
#define WS_CLASS "WebSocket"
#define WS_RECV_BUF_SIZE 16000

/* -------------------------------------------------------------- Network -- */

static int l_net_init(lua_State *L)
{
    int ret;
    if (lua_gettop(L) >= 4)
        ret = athena_net_init(luaL_checkstring(L, 1), luaL_checkstring(L, 2), luaL_checkstring(L, 3), luaL_checkstring(L, 4));
    else
        ret = athena_net_init_dhcp();
    if (ret != 0)
        return luaL_error(L, "network initialization failed");
    return 0;
}

static int l_net_get_config(lua_State *L)
{
    AthenaNetConfig config;
    if (athena_net_get_config(&config) != 0)
        return luaL_error(L, "unable to read network info");
    lua_createtable(L, 0, 4);
    athena_lua_set_str(L, "ip", config.ip);
    athena_lua_set_str(L, "netmask", config.netmask);
    athena_lua_set_str(L, "gateway", config.gateway);
    athena_lua_set_str(L, "dns", config.dns);
    return 1;
}

static int l_net_deinit(lua_State *L)
{
    athena_net_deinit();
    return 0;
}

static int l_net_resolve(lua_State *L)
{
    char ip[16];
    if (athena_net_resolve(luaL_checkstring(L, 1), ip, sizeof(ip)) != 0)
        return luaL_error(L, "unable to resolve '%s'", lua_tostring(L, 1));
    lua_pushstring(L, ip);
    return 1;
}

static const luaL_Reg net_funcs[] = {
    { "init", l_net_init }, { "getConfig", l_net_get_config },
    { "getHostbyName", l_net_resolve }, { "deinit", l_net_deinit },
    { NULL, NULL }
};

/* -------------------------------------------------------------- Request -- */

typedef struct {
    AthenaRequest *req;
} LuaRequest;

static AthenaRequest *check_req(lua_State *L, int idx)
{
    LuaRequest *r = athena_lua_check_object(L, idx, REQUEST_CLASS);
    if (!r->req)
        luaL_error(L, "request was freed");
    return r->req;
}

/* Keeps the string at `idx` alive in the request's field table. */
static const char *keep_string(lua_State *L, int obj, int idx, const char *key)
{
    const char *s = luaL_checkstring(L, idx);
    athena_lua_retain(L, obj, idx, key);
    return s;
}

static int l_req_new(lua_State *L)
{
    LuaRequest *r = athena_lua_new_object(L, REQUEST_CLASS, sizeof(LuaRequest));
    r->req = athena_request_create();
    if (!r->req)
        return luaL_error(L, "out of memory");
    return 1;
}

static int l_req_gc(lua_State *L)
{
    LuaRequest *r = athena_lua_check_object(L, 1, REQUEST_CLASS);
    if (r->req) {
        /* an async transfer still writes into the request: let it finish */
        if (r->req->tid != -1)
            while (!r->req->ready && !r->req->error)
                athena_request_poll(r->req, 999999999, 3000);
        /* body not taken with getAsyncData(); headers belong to responses */
        if (r->req->ready && r->req->chunk.memory && !r->req->save)
            free(r->req->chunk.memory);
        athena_request_destroy(r->req);
    }
    r->req = NULL;
    return 0;
}

static void push_response(lua_State *L, AthenaRequestResponse *resp)
{
    lua_createtable(L, 0, 3);
    lua_pushlstring(L, resp->body ? resp->body : "", resp->body_size);
    lua_setfield(L, -2, "text");
    athena_lua_set_int(L, "status_code", (int)resp->status_code);
    if (resp->headers)
        athena_lua_set_str(L, "headers", resp->headers);
    athena_request_response_free(resp);
}

static int request_error(lua_State *L, AthenaRequest *req, const char *fallback)
{
    return luaL_error(L, "%s", req->error ? req->error : fallback);
}

static int l_req_get(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    AthenaRequestResponse *resp = NULL;
    if (athena_request_get(req, luaL_checkstring(L, 2), &resp) != 0)
        return request_error(L, req, "network error");
    push_response(L, resp);
    return 1;
}

static int l_req_head(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    AthenaRequestResponse *resp = NULL;
    if (athena_request_head(req, luaL_checkstring(L, 2), &resp) != 0)
        return request_error(L, req, "network error");
    push_response(L, resp);
    return 1;
}

static int l_req_post(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    AthenaRequestResponse *resp = NULL;
    if (athena_request_post(req, luaL_checkstring(L, 2), luaL_checkstring(L, 3), &resp) != 0)
        return request_error(L, req, "network error");
    push_response(L, resp);
    return 1;
}

static int l_req_download(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    if (athena_request_download(req, luaL_checkstring(L, 2), luaL_checkstring(L, 3)) != 0)
        return request_error(L, req, "download: error creating file");
    return 0;
}

static void reset_async(AthenaRequest *req)
{
    req->ready = false;
    req->error = NULL;
    req->chunk.timer = 0;
    req->chunk.transferring = false;
    req->chunk.size = 0;
}

static int start_async(lua_State *L, AthenaRequest *req, const char *task)
{
    if (athena_request_run_async(req, task) != 0)
        return luaL_error(L, "%s: unable to create task", task);
    return 0;
}

static int l_req_async_get(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    reset_async(req);
    req->url = keep_string(L, 1, 2, "__url");
    req->method = ATHENA_REQUEST_GET;
    req->save = false;
    req->chunk.memory = malloc(1);
    return start_async(L, req, "Requests: Get");
}

static int l_req_async_post(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    reset_async(req);
    req->url = keep_string(L, 1, 2, "__url");
    req->postdata = keep_string(L, 1, 3, "__postdata");
    req->method = ATHENA_REQUEST_POST;
    req->save = false;
    req->chunk.memory = malloc(1);
    return start_async(L, req, "Requests: Post");
}

static int l_req_async_download(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    reset_async(req);
    req->url = keep_string(L, 1, 2, "__url");
    req->method = ATHENA_REQUEST_GET;
    req->save = true;
    req->chunk.fp = fopen(luaL_checkstring(L, 3), "wb");
    if (!req->chunk.fp)
        return luaL_error(L, "asyncDownload: error creating file");
    return start_async(L, req, "Requests: Download");
}

/* ready([timeout_ms [, transfer_timeout_ms]]) -- also what await(req) polls */
static int l_req_ready(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    bool ready = athena_request_poll(req, (int)luaL_optinteger(L, 2, 999999999), (int)luaL_optinteger(L, 3, 3000));
    if (req->error)
        return luaL_error(L, "%s", req->error);
    lua_pushboolean(L, ready);
    return 1;
}

static int l_req_get_async_data(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    AthenaRequestResponse *resp = athena_request_take_response(req);
    if (req->error)
        return luaL_error(L, "%s", req->error);
    if (!resp)
        return 0;
    lua_pushlstring(L, resp->body ? resp->body : "", resp->body_size);
    athena_request_response_free(resp);
    return 1;
}

static int l_req_get_async_size(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)check_req(L, 1)->chunk.size);
    return 1;
}

static int get_req_keepalive(lua_State *L) { lua_pushboolean(L, check_req(L, 1)->keepalive); return 1; }
static int set_req_keepalive(lua_State *L) { athena_request_set_keepalive(check_req(L, 1), athena_lua_tobool(L, 2)); return 0; }
static int get_req_timeout(lua_State *L) { lua_pushinteger(L, check_req(L, 1)->timeout_ms); return 1; }
static int set_req_timeout(lua_State *L) { athena_request_set_timeout(check_req(L, 1), (int)luaL_checkinteger(L, 2)); return 0; }
static int get_req_verifyTLS(lua_State *L) { lua_pushboolean(L, check_req(L, 1)->verify_tls); return 1; }
static int set_req_verifyTLS(lua_State *L) { athena_request_set_verify_tls(check_req(L, 1), athena_lua_tobool(L, 2)); return 0; }
static int get_req_followRedirects(lua_State *L) { lua_pushboolean(L, check_req(L, 1)->follow_redirects); return 1; }
static int set_req_followRedirects(lua_State *L) { athena_request_set_follow_redirects(check_req(L, 1), athena_lua_tobool(L, 2)); return 0; }

static int get_req_useragent(lua_State *L) { lua_pushstring(L, check_req(L, 1)->useragent); return 1; }
static int set_req_useragent(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    athena_request_set_useragent(req, keep_string(L, 1, 2, "__useragent"));
    return 0;
}

static int get_req_userpwd(lua_State *L) { lua_pushstring(L, check_req(L, 1)->userpwd); return 1; }
static int set_req_userpwd(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    athena_request_set_userpwd(req, keep_string(L, 1, 2, "__userpwd"));
    return 0;
}

static int get_req_headers(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    lua_createtable(L, req->headers_len, 0);
    for (int i = 0; i < req->headers_len; i++) {
        lua_pushstring(L, req->headers[i]);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int set_req_headers(lua_State *L)
{
    AthenaRequest *req = check_req(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = (int)luaL_len(L, 2);
    if (n > 16)
        return luaL_error(L, "16 headers is the maximum quantity");

    /* a private copy of the list keeps every header string referenced */
    lua_createtable(L, n, 0);
    const char *headers[16];
    for (int i = 0; i < n; i++) {
        lua_geti(L, 2, i + 1);
        headers[i] = luaL_checkstring(L, -1);
        lua_rawseti(L, -2, i + 1);
    }
    athena_lua_retain(L, 1, -1, "__headers");
    athena_request_set_headers(req, headers, n);
    return 0;
}

#define REQ_PROP(name) { #name, get_req_##name, set_req_##name }

static const athena_lua_prop req_props[] = {
    REQ_PROP(keepalive), REQ_PROP(useragent), REQ_PROP(userpwd), REQ_PROP(timeout),
    REQ_PROP(verifyTLS), REQ_PROP(followRedirects), REQ_PROP(headers),
    { NULL, NULL, NULL }
};

static const luaL_Reg req_methods[] = {
    { "get", l_req_get }, { "head", l_req_head }, { "post", l_req_post }, { "download", l_req_download },
    { "asyncGet", l_req_async_get }, { "asyncPost", l_req_async_post }, { "asyncDownload", l_req_async_download },
    { "getAsyncData", l_req_get_async_data }, { "getAsyncSize", l_req_get_async_size }, { "ready", l_req_ready },
    { NULL, NULL }
};

static const athena_lua_class req_class = {
    .name = REQUEST_CLASS, .methods = req_methods, .props = req_props, .gc = l_req_gc,
};

/* --------------------------------------------------------------- Socket -- */

typedef struct {
    AthenaSocket *sock;
} LuaSocket;

static AthenaSocket *check_sock(lua_State *L, int idx)
{
    LuaSocket *s = athena_lua_check_object(L, idx, SOCKET_CLASS);
    if (!s->sock)
        luaL_error(L, "socket is closed");
    return s->sock;
}

static int l_sock_new(lua_State *L)
{
    int family = (int)luaL_optinteger(L, 1, AF_INET);
    int protocol = (int)luaL_optinteger(L, 2, SOCK_STREAM);
    AthenaSocket *sock = athena_socket_create(family, protocol);
    if (!sock)
        return luaL_error(L, "could not create socket");
    LuaSocket *s = athena_lua_new_object(L, SOCKET_CLASS, sizeof(LuaSocket));
    s->sock = sock;
    return 1;
}

static int l_sock_close(lua_State *L)
{
    LuaSocket *s = athena_lua_check_object(L, 1, SOCKET_CLASS);
    if (s->sock)
        athena_socket_destroy(s->sock);
    s->sock = NULL;
    return 0;
}

static int l_sock_connect(lua_State *L)
{
    lua_pushinteger(L, athena_socket_connect(check_sock(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3)));
    return 1;
}

static int l_sock_bind(lua_State *L)
{
    lua_pushinteger(L, athena_socket_bind(check_sock(L, 1), luaL_checkstring(L, 2), (int)luaL_checkinteger(L, 3)));
    return 1;
}

static int l_sock_listen(lua_State *L)
{
    lua_pushinteger(L, athena_socket_listen(check_sock(L, 1)));
    return 1;
}

/* send(string | Buffer) */
static int l_sock_send(lua_State *L)
{
    uint32_t len;
    const uint8_t *data = athena_lua_to_bytes(L, 2, &len);
    if (!data)
        return luaL_argerror(L, 2, "string or Buffer expected");
    lua_pushinteger(L, athena_socket_send(check_sock(L, 1), data, len));
    return 1;
}

static int l_sock_recv(lua_State *L)
{
    AthenaSocket *sock = check_sock(L, 1);
    int len = (int)luaL_checkinteger(L, 2);
    luaL_Buffer b;
    char *buf = luaL_buffinitsize(L, &b, len > 0 ? len : 1);
    size_t received = 0;
    int ret = athena_socket_recv(sock, buf, len, &received);
    luaL_pushresultsize(&b, ret > 0 ? received : 0);
    return 1;
}

static const luaL_Reg sock_methods[] = {
    { "connect", l_sock_connect }, { "bind", l_sock_bind }, { "listen", l_sock_listen },
    { "send", l_sock_send }, { "recv", l_sock_recv }, { "close", l_sock_close },
    { NULL, NULL }
};

static const athena_lua_const sock_consts[] = {
    { "AF_INET", AF_INET }, { "SOCK_STREAM", SOCK_STREAM }, { "SOCK_DGRAM", SOCK_DGRAM }, { "SOCK_RAW", SOCK_RAW },
    { NULL, 0 }
};

static const athena_lua_class sock_class = {
    .name = SOCKET_CLASS, .methods = sock_methods, .gc = l_sock_close,
};

/* ------------------------------------------------------------ WebSocket -- */

typedef struct {
    AthenaWebSocket *ws;
    bool verify_tls;
} LuaWebSocket;

static AthenaWebSocket *check_ws(lua_State *L, int idx)
{
    LuaWebSocket *w = athena_lua_check_object(L, idx, WS_CLASS);
    if (!w->ws)
        luaL_error(L, "websocket is closed");
    return w->ws;
}

/* WebSocket(url [, { verifyTLS = bool }]) */
static int l_ws_new(lua_State *L)
{
    const char *url = luaL_checkstring(L, 1);
    bool verify = true;
    if (lua_istable(L, 2))
        athena_lua_field_bool(L, 2, "verifyTLS", &verify);

    AthenaWebSocket *ws = athena_websocket_connect(url, verify);
    if (!ws)
        return luaL_error(L, "WebSocket connect failed");
    LuaWebSocket *w = athena_lua_new_object(L, WS_CLASS, sizeof(LuaWebSocket));
    w->ws = ws;
    w->verify_tls = verify;
    return 1;
}

static int l_ws_gc(lua_State *L)
{
    LuaWebSocket *w = athena_lua_check_object(L, 1, WS_CLASS);
    if (w->ws)
        athena_websocket_destroy(w->ws);
    w->ws = NULL;
    return 0;
}

static int l_ws_close(lua_State *L)
{
    LuaWebSocket *w = athena_lua_check_object(L, 1, WS_CLASS);
    if (w->ws)
        athena_websocket_close(w->ws);
    w->ws = NULL;
    return 0;
}

/* send(data [, binary]) -- binary frames by default, like the JS binding */
static int l_ws_send(lua_State *L)
{
    AthenaWebSocket *ws = check_ws(L, 1);
    uint32_t len;
    const uint8_t *data = athena_lua_to_bytes(L, 2, &len);
    if (!data)
        return luaL_argerror(L, 2, "string or Buffer expected");
    bool binary = lua_isnoneornil(L, 3) ? true : athena_lua_tobool(L, 3);
    if (athena_websocket_send(ws, data, len, binary) != 0)
        return luaL_error(L, "WebSocket send failed");
    lua_pushinteger(L, len);
    return 1;
}

/* recv() -> string with the next message, or nil */
static int l_ws_recv(lua_State *L)
{
    AthenaWebSocket *ws = check_ws(L, 1);
    luaL_Buffer b;
    uint8_t *buf = (uint8_t *)luaL_buffinitsize(L, &b, WS_RECV_BUF_SIZE);
    size_t len = 0;
    if (athena_websocket_recv(ws, buf, WS_RECV_BUF_SIZE, &len) != 0)
        return 0;
    luaL_pushresultsize(&b, len);
    return 1;
}

static int get_ws_verifyTLS(lua_State *L)
{
    lua_pushboolean(L, ((LuaWebSocket *)athena_lua_check_object(L, 1, WS_CLASS))->verify_tls);
    return 1;
}

static const athena_lua_prop ws_props[] = {
    { "verifyTLS", get_ws_verifyTLS, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg ws_methods[] = {
    { "send", l_ws_send }, { "recv", l_ws_recv }, { "close", l_ws_close },
    { NULL, NULL }
};

static const athena_lua_class ws_class = {
    .name = WS_CLASS, .methods = ws_methods, .props = ws_props, .gc = l_ws_gc,
};

/* ---------------------------------------------------------------- open -- */

static void open_network(lua_State *L)
{
    athena_lua_register_class(L, &req_class);
    athena_lua_new_module(L, NULL, NULL, l_req_new);
    athena_lua_set_global_module(L, REQUEST_CLASS);

    athena_lua_register_class(L, &sock_class);
    athena_lua_new_module(L, NULL, sock_consts, l_sock_new);
    athena_lua_set_global_module(L, SOCKET_CLASS);

    athena_lua_register_class(L, &ws_class);
    athena_lua_new_module(L, NULL, NULL, l_ws_new);
    athena_lua_set_global_module(L, WS_CLASS);

    athena_lua_new_module(L, net_funcs, NULL, NULL);
    lua_pushvalue(L, -1);
    athena_lua_set_global_module(L, "Network");
}

#ifdef DYNAMIC_ATHENA_NETWORK
#include <iop_manager.h>
#include <netman.h>

extern unsigned char NETMAN_irx[];
extern unsigned int size_NETMAN_irx;
extern unsigned char SMAP_irx[];
extern unsigned int size_SMAP_irx;
extern void __ps2ipeeOpsInitializeImpl(void);

char *erl_id = "athena_network_lua";

/* Same boot sequence as the JS athena_network.erl: ps2ip glue + the
 * NETMAN/SMAP IOP drivers embedded in this relocatable. */
static void network_erl_boot(void)
{
    static bool booted = false;
    if (!booted) {
        __ps2ipeeOpsInitializeImpl();
        booted = true;
    }

    if (!iopman_search_module("NETMAN")) {
        module_entry *dev9 = iopman_search_module("ps2dev9");
        uint8_t dev9_deps[4] = { dev9 ? dev9->id : EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY };
        module_entry *netman = iopman_register_module_buffer("NETMAN", NETMAN, dev9_deps, NetManInit, NetManDeinit);

        uint8_t netman_deps[4] = { netman ? netman->id : EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY };
        iopman_register_module_buffer("SMAP", SMAP, netman_deps, NULL, NULL);

        module_entry *udpbd = iopman_search_module("smap_udpbd");
        if (udpbd && netman)
            iopman_add_incompatible_module(udpbd, netman);
    }
}

int lua_init_module(lua_State *L)
{
    network_erl_boot();
    open_network(L);
    return 1;
}
#else
void athena_lua_open_network(lua_State *L)
{
    open_network(L);
    lua_pop(L, 1);
}
#endif
