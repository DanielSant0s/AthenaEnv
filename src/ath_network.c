#include "ath_env.h"
#include <netman.h>
#include <ps2ip.h>
#include <curl/curl.h>

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory */
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

static int ethApplyNetIFConfig(int mode)
{
	int result;
	//By default, auto-negotiation is used.
	static int CurrentMode = NETMAN_NETIF_ETH_LINK_MODE_AUTO;

	if(CurrentMode != mode)
	{	//Change the setting, only if different.
		if((result = NetManSetLinkMode(mode)) == 0)
			CurrentMode = mode;
	}else
		result = 0;

	return result;
}

static void EthStatusCheckCb(s32 alarm_id, u16 time, void *common)
{
	iWakeupThread(*(int*)common);
}

static int WaitValidNetState(int (*checkingFunction)(void))
{
	int ThreadID, retry_cycles;

	// Wait for a valid network status;
	ThreadID = GetThreadId();
	for(retry_cycles = 0; checkingFunction() == 0; retry_cycles++)
	{	//Sleep for 1000ms.
		SetAlarm(1000 * 16, &EthStatusCheckCb, &ThreadID);
		SleepThread();

		if(retry_cycles >= 10)	//10s = 10*1000ms
			return -1;
	}

	return 0;
}

static int ethGetNetIFLinkStatus(void)
{
	return(NetManIoctl(NETMAN_NETIF_IOCTL_GET_LINK_STATUS, NULL, 0, NULL, 0) == NETMAN_NETIF_ETH_LINK_STATE_UP);
}

static int ethWaitValidNetIFLinkState(void)
{
	return WaitValidNetState(&ethGetNetIFLinkStatus);
}

static int ethGetDHCPStatus(void)
{
	t_ip_info ip_info;
	int result;

	if ((result = ps2ip_getconfig("sm0", &ip_info)) >= 0)
	{	//Check for a successful state if DHCP is enabled.
		if (ip_info.dhcp_enabled)
			result = (ip_info.dhcp_status == DHCP_STATE_BOUND || (ip_info.dhcp_status == DHCP_STATE_OFF));
		else
			result = -1;
	}

	return result;
}

static int ethWaitValidDHCPState(void)
{
	return WaitValidNetState(&ethGetDHCPStatus);
}

static int ethApplyIPConfig(int use_dhcp, const struct ip4_addr *ip, const struct ip4_addr *netmask, const struct ip4_addr *gateway, const struct ip4_addr *dns)
{
	t_ip_info ip_info;
	int result;

	//SMAP is registered as the "sm0" device to the TCP/IP stack.
	if ((result = ps2ip_getconfig("sm0", &ip_info)) >= 0)
	{
		const ip_addr_t *dns_curr;

		//Obtain the current DNS server settings.
		dns_curr = dns_getserver(0);

		//Check if it's the same. Otherwise, apply the new configuration.
		if ((use_dhcp != ip_info.dhcp_enabled)
		    ||	(!use_dhcp &&
			 (!ip_addr_cmp(ip, (struct ip4_addr *)&ip_info.ipaddr) ||
			 !ip_addr_cmp(netmask, (struct ip4_addr *)&ip_info.netmask) ||
			 !ip_addr_cmp(gateway, (struct ip4_addr *)&ip_info.gw) ||
			 !ip_addr_cmp(dns, dns_curr))))
		{
			if (use_dhcp)
			{
				ip_info.dhcp_enabled = 1;
			}
			else
			{	//Copy over new settings if DHCP is not used.
				ip_addr_set((struct ip4_addr *)&ip_info.ipaddr, ip);
				ip_addr_set((struct ip4_addr *)&ip_info.netmask, netmask);
				ip_addr_set((struct ip4_addr *)&ip_info.gw, gateway);

				ip_info.dhcp_enabled = 0;
			}

			//Update settings.
			result = ps2ip_setconfig(&ip_info);
			if (!use_dhcp)
				dns_setserver(0, dns);
		}
		else
			result = 0;
	}

	return result;
}

static JSValue athena_nw_init(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0 && argc != 4) return JS_ThrowSyntaxError(ctx, "wrong number of arguments.");
	
    struct ip4_addr IP, NM, GW, DNS;

    ip4_addr_set_zero(&IP);
	ip4_addr_set_zero(&NM);
	ip4_addr_set_zero(&GW);
	ip4_addr_set_zero(&DNS);

    NetManInit();

	if(ethApplyNetIFConfig(NETMAN_NETIF_ETH_LINK_MODE_AUTO) != 0) {
		return JS_ThrowSyntaxError(ctx, "Error: failed to set link mode.");
	}

    if (argc == 4){
        IP.addr = ipaddr_addr(JS_ToCString(ctx, argv[0]));
        NM.addr = ipaddr_addr(JS_ToCString(ctx, argv[1]));
        GW.addr = ipaddr_addr(JS_ToCString(ctx, argv[2]));
        DNS.addr = ipaddr_addr(JS_ToCString(ctx, argv[3]));
    }

    ps2ipInit(&IP, &NM, &GW);
    ethApplyIPConfig((argc == 4? 0 : 1), &IP, &NM, &GW, &DNS);

    if(ethWaitValidNetIFLinkState() != 0) {
	    return JS_ThrowSyntaxError(ctx, "Error: failed to get valid link status.\n");
	}

    if(argc == 4) return JS_UNDEFINED;

	if (ethWaitValidDHCPState() != 0)
	{
		return JS_ThrowSyntaxError(ctx, "DHCP failed\n.");
	}

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
		obj = JS_ThrowSyntaxError(ctx, "Unable to read IP address.\n");
	}

    return obj;
}

static JSValue athena_nw_deinit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	ps2ipDeinit();
	NetManDeinit();

    return 0;
}

static JSValue athena_nw_gethostbyname(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    const char* host = JS_ToCString(ctx, argv[0]);
    struct hostent *host_address = lwip_gethostbyname(host);
    
    if (host_address == NULL)
        return JS_ThrowSyntaxError(ctx, "Unable to resolve address.\n");

    return JS_NewString(ctx, inet_ntoa(*(struct in_addr*)host_address->h_addr));
}

static JSValue athena_nw_requests_get(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    CURL *curl;
    CURLcode res;

    struct MemoryStruct chunk;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, JS_ToCString(ctx, argv[0]));

        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
 
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
 
        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
    
        /* always cleanup */
        curl_easy_cleanup(curl);

        //free(chunk.memory);
  }
 
    curl_global_cleanup();
 
    return JS_NewStringLen(ctx, chunk.memory, chunk.size);
}

static JSValue athena_nw_requests_post(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    CURL *curl;
    CURLcode res;

    struct MemoryStruct chunk;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    unsigned int len = 0;

    curl_global_init(CURL_GLOBAL_ALL);
    
    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, JS_ToCString(ctx, argv[0]));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, JS_ToCStringLen(ctx, &len, argv[1]));

        /* if we do not provide POSTFIELDSIZE, libcurl will strlen() by
           itself */
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)len);

        /* send all data to this function  */
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

        /* we pass our 'chunk' struct to the callback function */
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);

        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
 
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        /* Perform the request, res will get the return code */
        res = curl_easy_perform(curl);
        /* Check for errors */
        if(res != CURLE_OK)
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));

        /* always cleanup */
        curl_easy_cleanup(curl);
    }

    return JS_NewStringLen(ctx, chunk.memory, chunk.size);
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("init", 4, athena_nw_init),
    JS_CFUNC_DEF("getConfig", 0, athena_nw_get_config),
    JS_CFUNC_DEF("getHostbyName", 1, athena_nw_gethostbyname),
    JS_CFUNC_DEF("get", 1, athena_nw_requests_get),
    JS_CFUNC_DEF("post", 2, athena_nw_requests_post),
    JS_CFUNC_DEF("deinit", 0, athena_nw_deinit),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_network_init(JSContext* ctx){
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Network");
}
