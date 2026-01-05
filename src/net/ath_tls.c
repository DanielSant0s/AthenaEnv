// Native TLS over lwIP using BearSSL
#include <ath_net.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#ifdef ATHENA_HAS_BEARSSL
#include <bearssl.h>
#include <dbgprintf.h>

typedef struct ath_tls_ctx {
    int sock;
    int timeout_ms;
    br_ssl_client_context sc;
    br_x509_minimal_context xc; // used when verification is enabled with anchors (not wired yet)
    br_sslio_context ioc;
    unsigned char iobuf[BR_SSL_BUFSIZE_BIDI];
    int verify;
} ath_tls_ctx_t_impl;

static int wait_rw(int fd, int write, int timeout_ms){
    fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int r = select(fd+1, write ? NULL : &fds, write ? &fds : NULL, NULL, (timeout_ms>0? &tv: NULL));
    return (r>0);
}

static int sock_read_cb(void *ctx, unsigned char *buf, size_t len) {
    ath_tls_ctx_t_impl *c = (ath_tls_ctx_t_impl*)ctx;
    if (c->timeout_ms > 0 && !wait_rw(c->sock, 0, c->timeout_ms)) return -1;
    ssize_t r = recv(c->sock, buf, len, 0);
    return (r <= 0) ? -1 : (int)r;
}

static int sock_write_cb(void *ctx, const unsigned char *buf, size_t len) {
    ath_tls_ctx_t_impl *c = (ath_tls_ctx_t_impl*)ctx;
    size_t off = 0;
    while (off < len) {
        if (c->timeout_ms > 0 && !wait_rw(c->sock, 1, c->timeout_ms)) return -1;
        ssize_t w = send(c->sock, buf + off, len - off, 0);
        if (w <= 0) return -1;
        off += (size_t)w;
    }
    return (int)len;
}

static int tcp_connect_host_tls(const char *host, int port, int timeout_ms, bool keepalive) {
    struct hostent *he = gethostbyname(host);
    if (!he) return -1;
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return -1;
    if (keepalive) {
        int opt = 1;
        setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    }
    int opt = 1;
    setsockopt(s, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    struct sockaddr_in addr; memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET; addr.sin_port = htons((uint16_t)port);
    memcpy(&addr.sin_addr, he->h_addr, he->h_length);

    int flags = fcntl(s, F_GETFL, 0);
    if (flags >= 0) fcntl(s, F_SETFL, flags | O_NONBLOCK);
    int rc = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        if (errno != EINPROGRESS) { close(s); return -1; }
        fd_set wfds; FD_ZERO(&wfds); FD_SET(s, &wfds);
        struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
        rc = select(s+1, NULL, &wfds, NULL, (timeout_ms>0? &tv: NULL));
        if (rc <= 0) { close(s); return -1; }
        int soerr = 0; socklen_t slen = sizeof(soerr);
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (void *)&soerr, &slen) < 0 || soerr != 0) { close(s); return -1; }
    }
    if (flags >= 0) fcntl(s, F_SETFL, flags);
    return s;
}

ath_tls_ctx_t *ath_tls_connect(const char *host, uint16_t port, bool verify, bool keepalive) {
    ath_tls_ctx_t_impl *ctx = (ath_tls_ctx_t_impl*)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->timeout_ms = 5000;
    ctx->verify = verify ? 1 : 0;

    ctx->sock = tcp_connect_host_tls(host, port, ctx->timeout_ms, keepalive);
    if (ctx->sock < 0) { free(ctx); return NULL; }

    // Initialize SSL client according to verification settings
    #include "ta_cabundle.c"

    if (ctx->verify) {
        if (TAs_NUM == 0) {
            dbgprintf("TLS: verify requested but TA bundle is empty\n");
            close(ctx->sock); free(ctx); return NULL;
        }
        br_ssl_client_init_full(&ctx->sc, &ctx->xc, TAs, TAs_NUM);

    } else {
        // No verification (compat mode)
        br_ssl_client_init_full(&ctx->sc, &ctx->xc, NULL, 0);
    }

    // Seed BearSSL RNG with some entropy from clock()/rand().
    // This is not cryptographically strong, mas é o melhor que temos
    // no ambiente PS2 sem fonte de entropia dedicada.
    unsigned char seed[32];
    unsigned int x = (unsigned int)clock() ^ (unsigned int)time(NULL);
    for (size_t i = 0; i < sizeof(seed); i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        seed[i] = (unsigned char)x;
    }
    br_ssl_engine_inject_entropy(&ctx->sc.eng, seed, sizeof(seed));

    br_ssl_engine_set_buffer(&ctx->sc.eng, ctx->iobuf, sizeof(ctx->iobuf), 1);
    br_sslio_init(&ctx->ioc, &ctx->sc.eng, sock_read_cb, ctx, sock_write_cb, ctx);
    br_ssl_client_reset(&ctx->sc, host, 0);

    // Trigger handshake by a flush
    if (br_sslio_flush(&ctx->ioc) != 0) {
        int err = br_ssl_engine_last_error(&ctx->sc.eng);
        printf("TLS: handshake error %d\n", err);
        ath_tls_close((ath_tls_ctx_t*)ctx);
        return NULL;
    }
    return (ath_tls_ctx_t*)ctx;
}

int ath_tls_read(ath_tls_ctx_t *c, void *buf, size_t len, int timeout_ms) {
    ath_tls_ctx_t_impl *ctx = (ath_tls_ctx_t_impl*)c;
    if (!ctx) return -1;
    int orig = ctx->timeout_ms;
    if (timeout_ms > 0) ctx->timeout_ms = timeout_ms;
    int r = br_sslio_read(&ctx->ioc, buf, len);
    if (timeout_ms > 0) ctx->timeout_ms = orig;
    return (r <= 0) ? -1 : r;
}

int ath_tls_write(ath_tls_ctx_t *c, const void *buf, size_t len, int timeout_ms) {
    ath_tls_ctx_t_impl *ctx = (ath_tls_ctx_t_impl*)c;
    if (!ctx) return -1;
    int orig = ctx->timeout_ms;
    if (timeout_ms > 0) ctx->timeout_ms = timeout_ms;
    int r = br_sslio_write_all(&ctx->ioc, buf, len);
    if (r == 0) r = br_sslio_flush(&ctx->ioc);
    if (timeout_ms > 0) ctx->timeout_ms = orig;
    return (r == 0) ? (int)len : -1;
}

void ath_tls_close(ath_tls_ctx_t *c) {
    ath_tls_ctx_t_impl *ctx = (ath_tls_ctx_t_impl*)c;
    if (!ctx) return;
    // Attempt to send close_notify
    br_sslio_flush(&ctx->ioc);
    if (ctx->sock >= 0) close(ctx->sock);
    free(ctx);
}

#else

typedef struct ath_tls_ctx { int _unused; } ath_tls_ctx_t_impl;

ath_tls_ctx_t *ath_tls_connect(const char *host, uint16_t port, bool verify) {
    (void)host; (void)port; (void)verify; return NULL;
}
int ath_tls_read(ath_tls_ctx_t *ctx, void *buf, size_t len, int timeout_ms) {
    (void)ctx; (void)buf; (void)len; (void)timeout_ms; return -1;
}
int ath_tls_write(ath_tls_ctx_t *ctx, const void *buf, size_t len, int timeout_ms) {
    (void)ctx; (void)buf; (void)len; (void)timeout_ms; return -1;
}
void ath_tls_close(ath_tls_ctx_t *ctx) { (void)ctx; }

#endif
