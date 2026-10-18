#include <ath_env.h>
#include <athena/net.h>
#include <macros.h>

static JSValue js_nw_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 0 && argc != 4)
		return JS_ThrowInternalError(ctx, "wrong number of arguments.");

	int ret;
	if (argc == 4) {
		const char *ip = JS_ToCString(ctx, argv[0]);
		const char *nm = JS_ToCString(ctx, argv[1]);
		const char *gw = JS_ToCString(ctx, argv[2]);
		const char *dns = JS_ToCString(ctx, argv[3]);
		ret = athena_net_init(ip, nm, gw, dns);
		JS_FreeCString(ctx, ip);
		JS_FreeCString(ctx, nm);
		JS_FreeCString(ctx, gw);
		JS_FreeCString(ctx, dns);
	} else {
		ret = athena_net_init_dhcp();
	}

	if (ret != 0)
		return JS_ThrowInternalError(ctx, "Network initialization failed.");

	return JS_UNDEFINED;
}

static JSValue js_nw_get_config(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	AthenaNetConfig config;
	if (athena_net_get_config(&config) != 0)
		return JS_ThrowInternalError(ctx, "Unable to read network info.\n");

	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "ip", JS_NewString(ctx, config.ip), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "netmask", JS_NewString(ctx, config.netmask), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "gateway", JS_NewString(ctx, config.gateway), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "dns", JS_NewString(ctx, config.dns), JS_PROP_C_W_E);
	return obj;
}

static JSValue js_nw_deinit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	athena_net_deinit();
	return JS_UNDEFINED;
}

static JSValue js_nw_gethostbyname(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *host = JS_ToCString(ctx, argv[0]);
	char ip[16];

	if (athena_net_resolve(host, ip, sizeof(ip)) != 0) {
		JS_FreeCString(ctx, host);
		return JS_ThrowInternalError(ctx, "Unable to resolve address.\n");
	}

	JSValue result = JS_NewString(ctx, ip);
	JS_FreeCString(ctx, host);
	return result;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("init", 4, js_nw_init),
	JS_CFUNC_DEF("getConfig", 0, js_nw_get_config),
	JS_CFUNC_DEF("getHostbyName", 1, js_nw_gethostbyname),
	JS_CFUNC_DEF("deinit", 0, js_nw_deinit),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_network_init(JSContext *ctx)
{
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Network");
}

#ifdef DYNAMIC_ATHENA_NETWORK
#include <iop_manager.h>
#include <netman.h>

extern unsigned char NETMAN_irx[];
extern unsigned int size_NETMAN_irx;
extern unsigned char SMAP_irx[];
extern unsigned int size_SMAP_irx;
extern void __ps2ipeeOpsInitializeImpl(void);

extern JSValue athena_request_create_class(JSContext *ctx);
extern JSValue athena_socket_create_class(JSContext *ctx);
extern JSValue athena_ws_create_class(JSContext *ctx);

char *erl_id = "athena_network";

static bool s_network_erl_booted = false;

static void athena_network_erl_boot(void)
{
	if (!s_network_erl_booted) {
		__ps2ipeeOpsInitializeImpl();
		s_network_erl_booted = true;
	}

	if (!iopman_search_module("NETMAN")) {
		module_entry *ps2dev9_entry = iopman_search_module("ps2dev9");
		uint8_t dev9_deps[4] = {
			ps2dev9_entry ? ps2dev9_entry->id : EMPTY_ENTRY,
			EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY
		};
		module_entry *NETMAN_entry = iopman_register_module_buffer(
			"NETMAN", NETMAN, dev9_deps, NetManInit, NetManDeinit);

		uint8_t netman_deps[4] = {
			NETMAN_entry ? NETMAN_entry->id : EMPTY_ENTRY,
			EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY
		};
		iopman_register_module_buffer("SMAP", SMAP, netman_deps, NULL, NULL);

		module_entry *smap_udpbd_entry = iopman_search_module("smap_udpbd");
		if (smap_udpbd_entry && NETMAN_entry) {
			iopman_add_incompatible_module(smap_udpbd_entry, NETMAN_entry);
		}
	}
}

static int erl_network_module_init(JSContext *ctx, JSModuleDef *m)
{
	if (JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs)) != 0)
		return -1;

	JSValue net_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, net_obj, module_funcs, countof(module_funcs));

	JSValue req_class = athena_request_create_class(ctx);
	JSValue sock_class = athena_socket_create_class(ctx);
	JSValue ws_class = athena_ws_create_class(ctx);

	JSValue global_obj = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global_obj, "Network", JS_DupValue(ctx, net_obj));
	JS_SetPropertyStr(ctx, global_obj, "Request", JS_DupValue(ctx, req_class));
	JS_SetPropertyStr(ctx, global_obj, "Socket", JS_DupValue(ctx, sock_class));
	JS_SetPropertyStr(ctx, global_obj, "WebSocket", JS_DupValue(ctx, ws_class));
	JS_FreeValue(ctx, global_obj);

	JS_SetModuleExport(ctx, m, "Network", net_obj);
	JS_SetModuleExport(ctx, m, "Request", req_class);
	JS_SetModuleExport(ctx, m, "Socket", sock_class);
	JS_SetModuleExport(ctx, m, "WebSocket", ws_class);
	return 0;
}

JSModuleDef *js_init_module(JSContext *ctx)
{
	athena_network_erl_boot();

	JSModuleDef *m = athena_push_module(ctx, erl_network_module_init, module_funcs, countof(module_funcs), "Network");
	if (m) {
		JS_AddModuleExport(ctx, m, "Network");
		JS_AddModuleExport(ctx, m, "Request");
		JS_AddModuleExport(ctx, m, "Socket");
		JS_AddModuleExport(ctx, m, "WebSocket");
	}
	return m;
}
#endif
