#include "ath_env.h"
#include <netman.h>
#include <ps2ip.h>

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

duk_ret_t athena_nw_init(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 4) return duk_generic_error(ctx, "wrong number of arguments.");
	
    struct ip4_addr IP, NM, GW, DNS;

    ip4_addr_set_zero(&IP);
	ip4_addr_set_zero(&NM);
	ip4_addr_set_zero(&GW);
	ip4_addr_set_zero(&DNS);

    NetManInit();

	if(ethApplyNetIFConfig(NETMAN_NETIF_ETH_LINK_MODE_AUTO) != 0) {
		return duk_generic_error(ctx, "Error: failed to set link mode.");
	}

    if (argc == 4){
        IP.addr = ipaddr_addr(duk_get_string(ctx, 0));
        NM.addr = ipaddr_addr(duk_get_string(ctx, 1));
        GW.addr = ipaddr_addr(duk_get_string(ctx, 2));
        DNS.addr = ipaddr_addr(duk_get_string(ctx, 3));
    }

    ps2ipInit(&IP, &NM, &GW);
    ethApplyIPConfig((argc == 4? 0 : 1), &IP, &NM, &GW, &DNS);

    if(ethWaitValidNetIFLinkState() != 0) {
	    return duk_generic_error(ctx, "Error: failed to get valid link status.\n");
	}

    if(argc == 4) return 0;

	if (ethWaitValidDHCPState() != 0)
	{
		return duk_generic_error(ctx, "DHCP failed\n.");
	}

	return 0;
}

duk_ret_t athena_nw_get_config(duk_context *ctx)
{
	t_ip_info ip_info;

	if (ps2ip_getconfig("sm0", &ip_info) >= 0)
	{
	    duk_idx_t obj_idx = duk_push_object(ctx);

	    duk_push_string(ctx, inet_ntoa(ip_info.ipaddr));
	    duk_put_prop_string(ctx, obj_idx, "ip");

	    duk_push_string(ctx, inet_ntoa(ip_info.netmask));
	    duk_put_prop_string(ctx, obj_idx, "netmask");

	    duk_push_string(ctx, inet_ntoa(ip_info.gw));
	    duk_put_prop_string(ctx, obj_idx, "gateway");

	    duk_push_string(ctx, inet_ntoa(*dns_getserver(0)));
	    duk_put_prop_string(ctx, obj_idx, "dns");
	}
	else
	{
		return duk_generic_error(ctx, "Unable to read IP address.\n");
	}

    return 1;
}

duk_ret_t athena_nw_deinit(duk_context *ctx)
{
	ps2ipDeinit();
	NetManDeinit();

    return 0;
}

DUK_EXTERNAL duk_ret_t dukopen_network(duk_context *ctx) {
    const duk_function_list_entry module_funcs[] = {
        { "init",                athena_nw_init,       DUK_VARARGS },
        { "getConfig",           athena_nw_get_config,           0 },
        { "deinit",              athena_nw_deinit,               0 },
        { NULL, NULL, 0 }
    };

    duk_push_object(ctx);  /* module result */
    duk_put_function_list(ctx, -1, module_funcs);

    return 1;  /* return module value */
}


void athena_network_init(duk_context* ctx){
	push_athena_module(dukopen_network,   		   "Network");

	//duk_push_uint(ctx, PAD_SELECT);
	//duk_put_global_string(ctx, "PAD_SELECT");
}
