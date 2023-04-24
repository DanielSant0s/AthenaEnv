
#include "include/network.h"

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory */
    dbgprintf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}


size_t AsyncWriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  mem->memory = realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    /* out of memory */
    dbgprintf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  mem->timer = clock();
  mem->transferring = true;

  return realsize;
}

size_t AsyncWriteFileCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;

  size_t written = fwrite(contents, size, nmemb, mem->fp);

  mem->size += written;
  mem->timer = clock();
  mem->transferring = true;

  return written;
}

int ethApplyNetIFConfig(int mode)
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

int ethWaitValidNetIFLinkState(void)
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

int ethWaitValidDHCPState(void)
{
	return WaitValidNetState(&ethGetDHCPStatus);
}

int ethApplyIPConfig(int use_dhcp, const struct ip4_addr *ip, const struct ip4_addr *netmask, const struct ip4_addr *gateway, const struct ip4_addr *dns)
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