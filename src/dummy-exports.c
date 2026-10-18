#ifdef __cplusplus
extern "C" {
#endif

extern void AddDmacHandler2(void);
extern void SetAlarm(void);
extern void __bswapdi2(void);
extern void __multi3(void);
extern void _libcglue_fdman_inet_ops(void);
extern void _libcglue_fdman_socket_ops(void);
extern void abs(void);
extern void aligned_alloc(void);
extern void arc4random(void);
extern void arc4random_buf(void);
extern void atol(void);
extern void bind(void);
extern void connect(void);
extern void dmaKit_send(void);
extern void fchmod(void);
extern void fmaxf(void);
extern void fminf(void);
extern void gethostbyname(void);
extern void getsockopt(void);
extern void gzclose(void);
extern void gzeof(void);
extern void gzopen(void);
extern void gzread(void);
extern void hypotf(void);
extern void iDisableDmac(void);
extern void listen(void);
extern void recv(void);
extern void remove(void);
extern void rewind(void);
extern void select(void);
extern void send(void);
extern void setsockopt(void);
extern void socket(void);
extern void strncasecmp(void);
extern void time(void);

struct export_list_t {
    char * name;
    void (*pointer)();
} export_list[] = {
    { "AddDmacHandler2", AddDmacHandler2 },
    { "SetAlarm", SetAlarm },
    { "__bswapdi2", __bswapdi2 },
    { "__multi3", __multi3 },
    { "_libcglue_fdman_inet_ops", _libcglue_fdman_inet_ops },
    { "_libcglue_fdman_socket_ops", _libcglue_fdman_socket_ops },
    { "abs", abs },
    { "aligned_alloc", aligned_alloc },
    { "arc4random", arc4random },
    { "arc4random_buf", arc4random_buf },
    { "atol", atol },
    { "bind", bind },
    { "connect", connect },
    { "dmaKit_send", dmaKit_send },
    { "fchmod", fchmod },
    { "fmaxf", fmaxf },
    { "fminf", fminf },
    { "gethostbyname", gethostbyname },
    { "getsockopt", getsockopt },
    { "gzclose", gzclose },
    { "gzeof", gzeof },
    { "gzopen", gzopen },
    { "gzread", gzread },
    { "hypotf", hypotf },
    { "iDisableDmac", iDisableDmac },
    { "listen", listen },
    { "recv", recv },
    { "remove", remove },
    { "rewind", rewind },
    { "select", select },
    { "send", send },
    { "setsockopt", setsockopt },
    { "socket", socket },
    { "strncasecmp", strncasecmp },
    { "time", time },
    { 0, 0 }
};

#ifdef __cplusplus
}
#endif
