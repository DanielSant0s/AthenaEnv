#include "../ath_env.h"
#include <netman.h>
#include <ps2ip.h>
#include <curl/curl.h>
#include <loadfile.h>
#include <pthread.h>

struct MemoryStruct {
    union {
     char *memory;
     FILE* fp;
    };
    size_t size;
    clock_t timer;
};

size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp);

int ethApplyNetIFConfig(int mode);
int ethWaitValidNetIFLinkState(void);
int ethWaitValidDHCPState(void);
int ethApplyIPConfig(int use_dhcp, const struct ip4_addr *ip, const struct ip4_addr *netmask, const struct ip4_addr *gateway, const struct ip4_addr *dns);