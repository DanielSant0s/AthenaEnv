// Native WebSocket client over lwIP/BearSSL
#include <ath_net.h>
#include <network.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#ifdef ATHENA_HAS_BEARSSL
#include <bearssl.h>
#endif

typedef struct ath_ws_ctx {
    int sock;
    int is_tls;
    int timeout_ms;
    int verify_tls;
    ath_tls_ctx_t *tls;
} ath_ws_ctx_impl;

static int to_lower(int c){ return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }

static int wait_rw(int fd, int write, int timeout_ms){
    fd_set fds; FD_ZERO(&fds); FD_SET(fd, &fds);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int r = select(fd+1, write ? NULL : &fds, write ? &fds : NULL, NULL, (timeout_ms>0? &tv: NULL));
    return (r>0);
}

static int parse_ws_url(const char *url, int *is_secure, const char **host, int *port, const char **path) {
    static char s_host[256];
    const char *p = strstr(url, "://");
    if (!p) return -1;
    size_t scheme_len = (size_t)(p - url);
    int is_wss = (scheme_len==3 && strncasecmp(url, "wss", 3)==0);
    int is_ws  = (scheme_len==2 && strncasecmp(url, "ws", 2)==0);
    if (!is_ws && !is_wss) return -1;
    *is_secure = is_wss;

    const char *after = p + 3;
    const char *slash = strchr(after, '/');
    if (!slash) slash = url + strlen(url);
    const char *colon = strchr(after, ':');
    const char *host_end = (colon && colon < slash) ? colon : slash;

    size_t host_len = (size_t)(host_end - after);
    if (host_len >= sizeof(s_host)) host_len = sizeof(s_host) - 1;
    memcpy(s_host, after, host_len);
    s_host[host_len] = '\0';
    *host = s_host;

    if (colon && colon < slash) {
        *port = atoi(colon + 1);
    } else {
        *port = is_wss ? 443 : 80;
    }

    *path = (*slash) ? slash : "/";
    return 0;
}

static int tcp_connect_ws(const char *host, int port, int timeout_ms) {
    struct hostent *he = gethostbyname(host);
    if (!he) return -1;
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) return -1;
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

static void ws_make_key(char out_b64[25]) {
    unsigned char rnd[16];
    unsigned int x = (unsigned int)clock() ^ (unsigned int)time(NULL);
    for (int i = 0; i < 16; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        rnd[i] = (unsigned char)x;
    }
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int j = 0;
    for (int i = 0; i < 16;) {
        uint32_t a = i < 16 ? rnd[i++] : 0;
        uint32_t b = i < 16 ? rnd[i++] : 0;
        uint32_t c = i < 16 ? rnd[i++] : 0;
        uint32_t triple = (a<<16) | (b<<8) | c;
        out_b64[j++] = tbl[(triple>>18)&0x3F];
        out_b64[j++] = tbl[(triple>>12)&0x3F];
        out_b64[j++] = tbl[(triple>>6)&0x3F];
        out_b64[j++] = tbl[triple&0x3F];
    }
    out_b64[j] = '\0';
}

static int ws_readline(ath_ws_ctx_impl *ctx, char *line, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        unsigned char ch;
        int r;
        if (ctx->is_tls) {
            r = ath_tls_read(ctx->tls, &ch, 1, ctx->timeout_ms);
        } else {
            if (!wait_rw(ctx->sock, 0, ctx->timeout_ms)) return -1;
            r = recv(ctx->sock, &ch, 1, 0);
        }
        if (r <= 0) return -1;
        line[i++] = (char)ch;
        if (ch == '\n') break;
    }
    line[i] = '\0';
    return (int)i;
}

ath_ws_ctx_t *ath_ws_connect(const char *url, bool verify_tls) {
    int is_secure = 0;
    const char *host = NULL, *path = NULL;
    int port = 0;
    if (parse_ws_url(url, &is_secure, &host, &port, &path) != 0) {
        dbgprintf("WS: invalid URL %s\n", url);
        return NULL;
    }

    ath_ws_ctx_impl *ctx = (ath_ws_ctx_impl*)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;
    ctx->timeout_ms = 5000;
    ctx->is_tls = is_secure;
    ctx->verify_tls = verify_tls ? 1 : 0;

    if (is_secure) {
        ctx->tls = ath_tls_connect(host, (uint16_t)port, ctx->verify_tls != 0, 1);
        if (!ctx->tls) { free(ctx); return NULL; }
    } else {
        ctx->sock = tcp_connect_ws(host, port, ctx->timeout_ms);
        if (ctx->sock < 0) { free(ctx); return NULL; }
    }

    char key[25];
    ws_make_key(key);

    char req[512];
    int n = snprintf(req, sizeof(req),
                     "GET %s HTTP/1.1\r\n"
                     "Host: %s:%d\r\n"
                     "Upgrade: websocket\r\n"
                     "Connection: Upgrade\r\n"
                     "Sec-WebSocket-Key: %s\r\n"
                     "Sec-WebSocket-Version: 13\r\n\r\n",
                     path, host, port, key);
    if (n <= 0 || n >= (int)sizeof(req)) { ath_ws_close((ath_ws_ctx_t*)ctx); return NULL; }

    int wr;
    if (ctx->is_tls) {
        wr = ath_tls_write(ctx->tls, req, (size_t)n, ctx->timeout_ms);
    } else {
        wr = send(ctx->sock, req, (size_t)n, 0);
    }
    if (wr < 0) { ath_ws_close((ath_ws_ctx_t*)ctx); return NULL; }

    char line[512];
    if (ws_readline(ctx, line, sizeof(line)) <= 0) { ath_ws_close((ath_ws_ctx_t*)ctx); return NULL; }
    if (strncmp(line, "HTTP/1.1 101", 12) != 0 && strncmp(line, "HTTP/1.0 101", 12) != 0) {
        dbgprintf("WS: bad status %s\n", line);
        ath_ws_close((ath_ws_ctx_t*)ctx);
        return NULL;
    }

    // Read headers until blank line
    while (1) {
        int r = ws_readline(ctx, line, sizeof(line));
        if (r <= 0) { ath_ws_close((ath_ws_ctx_t*)ctx); return NULL; }
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) break;
    }

    return (ath_ws_ctx_t*)ctx;
}

static int ws_read_raw(ath_ws_ctx_impl *ctx, void *buf, size_t len) {
    if (ctx->is_tls) {
        int r = ath_tls_read(ctx->tls, buf, len, ctx->timeout_ms);
        return r;
    } else {
        if (!wait_rw(ctx->sock, 0, ctx->timeout_ms)) return -1;
        ssize_t r = recv(ctx->sock, buf, len, 0);
        return (int)r;
    }
}

static int ws_write_raw(ath_ws_ctx_impl *ctx, const void *buf, size_t len) {
    if (ctx->is_tls) {
        return ath_tls_write(ctx->tls, buf, len, ctx->timeout_ms);
    } else {
        size_t off = 0; const unsigned char *p = buf;
        while (off < len) {
            if (!wait_rw(ctx->sock, 1, ctx->timeout_ms)) return -1;
            ssize_t w = send(ctx->sock, p+off, len-off, 0);
            if (w <= 0) return -1;
            off += (size_t)w;
        }
        return (int)len;
    }
}

int ath_ws_recv(ath_ws_ctx_t *ws, uint8_t *buf, size_t buflen, size_t *out_len) {
    ath_ws_ctx_impl *ctx = (ath_ws_ctx_impl*)ws;
    if (!ctx || !buf || buflen == 0 || !out_len) return -1;
    *out_len = 0;

    unsigned char hdr[2];
    if (ws_read_raw(ctx, hdr, 2) != 2) return -1;
    unsigned char fin = hdr[0] & 0x80;
    unsigned char opcode = hdr[0] & 0x0F;
    unsigned char mask = hdr[1] & 0x80;
    uint64_t len = hdr[1] & 0x7F;

    if (!fin) return -1; // no fragmentation support yet
    if (opcode == 0x8) return -1; // close frame

    if (len == 126) {
        unsigned char ext[2];
        if (ws_read_raw(ctx, ext, 2) != 2) return -1;
        len = (ext[0] << 8) | ext[1];
    } else if (len == 127) {
        unsigned char ext[8];
        if (ws_read_raw(ctx, ext, 8) != 8) return -1;
        len = 0;
        for (int i = 0; i < 8; i++) len = (len << 8) | ext[i];
    }

    if (mask) {
        unsigned char mkey[4];
        if (ws_read_raw(ctx, mkey, 4) != 4) return -1;
    }

    if (len > buflen) return -1;
    if (ws_read_raw(ctx, buf, len) != (int)len) return -1;
    *out_len = (size_t)len;
    return 0;
}

int ath_ws_send(ath_ws_ctx_t *ws, const uint8_t *buf, size_t len, int is_binary) {
    ath_ws_ctx_impl *ctx = (ath_ws_ctx_impl*)ws;
    if (!ctx || !buf) return -1;

    unsigned char hdr[14];
    size_t hlen = 0;
    unsigned char opcode = is_binary ? 0x2 : 0x1;
    hdr[0] = 0x80 | opcode; // FIN + opcode

    if (len <= 125) {
        hdr[1] = 0x80 | (unsigned char)len;
        hlen = 2;
    } else if (len <= 0xFFFF) {
        hdr[1] = 0x80 | 126;
        hdr[2] = (unsigned char)((len >> 8) & 0xFF);
        hdr[3] = (unsigned char)(len & 0xFF);
        hlen = 4;
    } else {
        hdr[1] = 0x80 | 127;
        for (int i = 0; i < 8; i++) hdr[2+i] = (unsigned char)((len >> (56 - 8*i)) & 0xFF);
        hlen = 10;
    }

    unsigned char mkey[4];
    unsigned int x = (unsigned int)clock() ^ (unsigned int)time(NULL);
    for (int i = 0; i < 4; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        mkey[i] = (unsigned char)x;
    }

    memcpy(hdr + hlen, mkey, 4);
    hlen += 4;

    if (ws_write_raw(ctx, hdr, hlen) < 0) return -1;

    uint8_t *masked = malloc(len);
    if (!masked) return -1;
    for (size_t i = 0; i < len; i++) masked[i] = buf[i] ^ mkey[i & 3];
    int rc = ws_write_raw(ctx, masked, len) < 0 ? -1 : 0;
    free(masked);
    return rc;
}

void ath_ws_close(ath_ws_ctx_t *ws) {
    ath_ws_ctx_impl *ctx = (ath_ws_ctx_impl*)ws;
    if (!ctx) return;
    if (ctx->is_tls && ctx->tls) {
        ath_tls_close(ctx->tls);
    } else if (ctx->sock >= 0) {
        close(ctx->sock);
    }
    free(ctx);
}
