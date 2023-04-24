#include "../ath_env.h"
#include <netman.h>
#include <ps2ip.h>
#include <curl/curl.h>
#include <loadfile.h>
#include <pthread.h>

extern CURL* curl; // REWORK IT LATER, ADD GET AND SET FUNCTIONS

struct MemoryStruct {
    union {
     char *memory;
     FILE* fp;
    };
    size_t size;
    bool transferring;
    clock_t timer;
};

enum RequestMethods {
    ATHENA_GET = 0,
    ATHENA_POST,
    ATHENA_HEAD,
};

typedef struct
{
    int tid;
    bool ready;
    bool save;
    int method;
    const char* url;
    const char* error;
    long keepalive;
    const char* userpwd;
    const char* useragent;
    struct MemoryStruct chunk;
    long response_code;
    char* headers[16];
    int headers_len;
} JSRequestData;

void requestThread(void* data);

int ethApplyNetIFConfig(int mode);
int ethWaitValidNetIFLinkState(void);
int ethWaitValidDHCPState(void);
int ethApplyIPConfig(int use_dhcp, const struct ip4_addr *ip, const struct ip4_addr *netmask, const struct ip4_addr *gateway, const struct ip4_addr *dns);