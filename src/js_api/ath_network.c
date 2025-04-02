#include <network.h>
#include <taskman.h>
#include <time.h>

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

    return JS_UNDEFINED;
}

static JSValue athena_nw_gethostbyname(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    const char* host = JS_ToCString(ctx, argv[0]);
    struct hostent *host_address = gethostbyname(host);
    
    if (host_address == NULL)
        return JS_ThrowInternalError(ctx, "Unable to resolve address.\n");

    return JS_NewString(ctx, ip4addr_ntoa((struct in_addr*)host_address->h_addr));
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("init", 4, athena_nw_init),
    JS_CFUNC_DEF("getConfig", 0, athena_nw_get_config),
    JS_CFUNC_DEF("getHostbyName", 1, athena_nw_gethostbyname),
    JS_CFUNC_DEF("deinit", 0, athena_nw_deinit),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_network_init(JSContext* ctx){
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Network");
}
