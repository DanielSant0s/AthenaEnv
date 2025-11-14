// Native HTTP client (initial implementation: HTTP only, no TLS yet)
#include <ath_net.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

#include <dbgprintf.h>

#define ATH_HTTP_MAX_REDIRECTS 3
#define ATH_HTTP_RBUF 4096

typedef struct {
    int sock;
    uint8_t buf[ATH_HTTP_RBUF];
    size_t len;
    size_t off;
    int timeout_ms;
} ath_http_conn_t;

static int append_header(ath_http_response_t *resp, const char *line) {
    if (!resp) return 0;
    size_t line_len = strlen(line);
    size_t add = line_len + 1; // + '\n'
    size_t old_len = resp->headers ? strlen(resp->headers) : 0;
    char *nh = realloc(resp->headers, old_len + add + 1);
    if (!nh) return -1;
    resp->headers = nh;
    memcpy(resp->headers + old_len, line, line_len);
    resp->headers[old_len + line_len] = '\n';
    resp->headers[old_len + line_len + 1] = '\0';
    return 0;
}

static int to_lower(int c){ return (c >= 'A' && c <= 'Z') ? (c + 32) : c; }
static int ci_strncmp(const char *a, const char *b, size_t n) {
    for (size_t i=0;i<n;i++){
        int ca = to_lower((unsigned char)a[i]);
        int cb = to_lower((unsigned char)b[i]);
        if (ca!=cb) return ca-cb;
        if (a[i]==0 || b[i]==0) break;
    }
    return 0;
}

static int wait_readable(int fd, int timeout_ms){
    fd_set rfds; FD_ZERO(&rfds); FD_SET(fd, &rfds);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int r = select(fd+1, &rfds, NULL, NULL, (timeout_ms>0? &tv: NULL));
    return (r>0);
}

static int wait_writable(int fd, int timeout_ms){
    fd_set wfds; FD_ZERO(&wfds); FD_SET(fd, &wfds);
    struct timeval tv; tv.tv_sec = timeout_ms/1000; tv.tv_usec = (timeout_ms%1000)*1000;
    int r = select(fd+1, NULL, &wfds, NULL, (timeout_ms>0? &tv: NULL));
    return (r>0);
}

static int conn_read(ath_http_conn_t *c, void *dst, size_t want) {
    size_t got = 0;
    uint8_t *out = (uint8_t*)dst;
    while (got < want) {
        if (c->off < c->len) {
            size_t avail = c->len - c->off;
            size_t cp = (want-got < avail) ? (want-got) : avail;
            memcpy(out+got, c->buf + c->off, cp);
            c->off += cp; got += cp;
            continue;
        }
        // refill after waiting for readability
        if (!wait_readable(c->sock, c->timeout_ms)) return -1;
        ssize_t r = recv(c->sock, c->buf, sizeof(c->buf), 0);
        if (r <= 0) return -1;
        c->off = 0; c->len = (size_t)r;
    }
    return (int)got;
}

static int conn_readline(ath_http_conn_t *c, char *line, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        if (c->off >= c->len) {
            if (!wait_readable(c->sock, c->timeout_ms)) return -1;
            ssize_t r = recv(c->sock, c->buf, sizeof(c->buf), 0);
            if (r <= 0) return -1;
            c->off = 0; c->len = (size_t)r;
        }
        char ch = (char)c->buf[c->off++];
        line[i++] = ch;
        if (ch == '\n') break;
    }
    line[i] = '\0';
    return (int)i;
}

static int parse_url(const char *url, const char **scheme, const char **host, int *port, const char **path) {
    static char s_host[256];
    const char *p = strstr(url, "://");
    if (!p) return -1;
    *scheme = url;
    const char *after = p + 3;
    size_t scheme_len = (size_t)(p - url);
    int is_https = (scheme_len==5 && ci_strncmp(url, "https", 5)==0);
    int is_http  = (scheme_len==4 && ci_strncmp(url, "http", 4)==0);
    if (!is_http && !is_https) return -1;

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
        *port = is_https ? 443 : 80;
    }

    *path = (*slash) ? slash : "/";
    return is_https ? 2 : 1; // 2=https, 1=http
}

static int tcp_connect_host(const char *host, int port, int timeout_ms, bool keepalive) {
    dbgprintf("HTTP: resolving host %s\n", host);
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

    if (timeout_ms <= 0) timeout_ms = 5000;
    // Non-blocking connect with select-based timeout
    int flags = fcntl(s, F_GETFL, 0);
    if (flags >= 0) fcntl(s, F_SETFL, flags | O_NONBLOCK);

    dbgprintf("HTTP: connecting %s:%d (timeout %dms)\n", host, port, timeout_ms);
    int rc = connect(s, (struct sockaddr*)&addr, sizeof(addr));
    if (rc < 0) {
        if (errno != EINPROGRESS) { dbgprintf("HTTP: connect immediate error %d\n", errno); close(s); return -1; }
        if (!wait_writable(s, timeout_ms)) { dbgprintf("HTTP: connect timeout\n"); close(s); return -1; }
        int soerr = 0; socklen_t slen = sizeof(soerr);
        if (getsockopt(s, SOL_SOCKET, SO_ERROR, (void *)&soerr, &slen) < 0 || soerr != 0) { dbgprintf("HTTP: SO_ERROR=%d\n", soerr); close(s); return -1; }
    }
    if (flags >= 0) fcntl(s, F_SETFL, flags);
    dbgprintf("HTTP: connected\n");
    return s;
}

static int write_all(int fd, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t*)buf; size_t off = 0;
    while (off < len) {
        ssize_t w = send(fd, p+off, len-off, 0);
        if (w <= 0) return -1;
        off += (size_t)w;
    }
    return 0;
}

static void trim_crlf(char *s) {
    size_t n = strlen(s);
    while (n>0 && (s[n-1]=='\r' || s[n-1]=='\n')) { s[--n]='\0'; }
}

static int hex_to_int(const char *s) {
    int v = 0; const char *p = s;
    while (*p) {
        char c = *p; int d;
        if (c>='0'&&c<='9') d=c-'0';
        else if (c>='a'&&c<='f') d=10+(c-'a');
        else if (c>='A'&&c<='F') d=10+(c-'A');
        else break;
        v = (v<<4) | d; p++;
    }
    return v;
}

static int send_basic_auth(int fd, const char *userpwd) {
    // Very small Base64 encoder for Basic auth
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t inlen = strlen(userpwd);
    size_t outlen = 4 * ((inlen + 2) / 3);
    char *out = (char*)malloc(outlen + 1);
    if (!out) return -1;
    size_t i=0, j=0;
    while (i < inlen) {
        size_t left = inlen - i;
        size_t chunk = left >= 3 ? 3 : left;
        uint32_t a = (unsigned char)userpwd[i++];
        uint32_t b = (chunk > 1) ? (unsigned char)userpwd[i++] : 0;
        uint32_t c = (chunk > 2) ? (unsigned char)userpwd[i++] : 0;
        uint32_t triple = (a<<16) | (b<<8) | c;
        out[j++] = tbl[(triple>>18)&0x3F];
        out[j++] = tbl[(triple>>12)&0x3F];
        out[j++] = (chunk > 1) ? tbl[(triple>>6)&0x3F] : '=';
        out[j++] = (chunk > 2) ? tbl[triple&0x3F] : '=';
    }
    out[j] = '\0';
    int rc = 0;
    char hdr[256];
    snprintf(hdr, sizeof(hdr), "Authorization: Basic %s\r\n", out);
    if (write_all(fd, hdr, strlen(hdr)) != 0) rc = -1;
    free(out);
    return rc;
}

static int send_basic_auth_tls(ath_tls_ctx_t *tls, const char *userpwd, int timeout_ms) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t inlen = strlen(userpwd);
    size_t outlen = 4 * ((inlen + 2) / 3);
    char *out = (char*)malloc(outlen + 1);
    if (!out) return -1;
    size_t i=0, j=0;
    while (i < inlen) {
        size_t left = inlen - i;
        size_t chunk = left >= 3 ? 3 : left;
        uint32_t a = (unsigned char)userpwd[i++];
        uint32_t b = (chunk > 1) ? (unsigned char)userpwd[i++] : 0;
        uint32_t c = (chunk > 2) ? (unsigned char)userpwd[i++] : 0;
        uint32_t triple = (a<<16) | (b<<8) | c;
        out[j++] = tbl[(triple>>18)&0x3F];
        out[j++] = tbl[(triple>>12)&0x3F];
        out[j++] = (chunk > 1) ? tbl[(triple>>6)&0x3F] : '=';
        out[j++] = (chunk > 2) ? tbl[triple&0x3F] : '=';
    }
    out[j] = '\0';
    int rc = 0;
    const char prefix[] = "Authorization: Basic ";
    if (ath_tls_write(tls, prefix, sizeof(prefix)-1, timeout_ms) < 0) rc = -1;
    if (!rc && ath_tls_write(tls, out, j, timeout_ms) < 0) rc = -1;
    if (!rc && ath_tls_write(tls, "\r\n", 2, timeout_ms) < 0) rc = -1;
    free(out);
    return rc;
}

static int perform_once(const ath_http_request_t *req, ath_http_response_t *resp) {
    const char *scheme=NULL,*host=NULL,*path=NULL; int port=0;
    int sch = parse_url(req->url, &scheme, &host, &port, &path);
    if (sch < 0) { resp->error = "Invalid URL"; dbgprintf("HTTP: invalid URL %s\n", req->url); return -1; }
    dbgprintf("HTTP: url='%s' host='%s' port=%d path='%s'\n", req->url, host, port, path);

    int s = -1;
    ath_tls_ctx_t *tls = NULL;

    if (sch == 2) {
        tls = ath_tls_connect(host, (uint16_t)port, req->verify_tls != 0, req->keepalive != 0);
        if (!tls) { resp->error = "TLS connect failed"; return -1; }
    } else
    {
        s = tcp_connect_host(host, port, req->timeout_ms, req->keepalive != 0);
        if (s < 0) { resp->error = "Connect failed"; return -1; }
    }

    // Build request line and headers
    const char *method = (req->method==ATH_HTTP_POST?"POST":(req->method==ATH_HTTP_HEAD?"HEAD":"GET"));
    char head[1024];
    int n = snprintf(head, sizeof(head), "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n",
                     method, path, host);
    if (n < 0 || n >= (int)sizeof(head)) { if (s>=0) close(s); if (tls) ath_tls_close(tls); resp->error = "Header build failed"; dbgprintf("HTTP: header build failed\n"); return -1; }
#ifdef ATHENA_HAS_BEARSSL
    if (tls) {
        if (ath_tls_write(tls, head, (size_t)n, req->timeout_ms) < 0) { resp->error = "Send failed"; dbgprintf("HTTP: send base headers failed (TLS)\n"); ath_tls_close(tls); return -1; }
    } else
#endif
    {
        if (write_all(s, head, (size_t)n) != 0) { if (s>=0) close(s); resp->error = "Send failed"; dbgprintf("HTTP: send base headers failed\n"); return -1; }
    }
    dbgprintf("HTTP: sent request line and base headers\n");

    // User-Agent
    if (req->useragent && req->useragent[0]) {
        char ua[256];
        int m = snprintf(ua, sizeof(ua), "User-Agent: %s\r\n", req->useragent);
        if (m<0) { if (s>=0) close(s); if (tls) ath_tls_close(tls); resp->error = "Send failed"; dbgprintf("HTTP: UA build failed\n"); return -1; }
#ifdef ATHENA_HAS_BEARSSL
        if (tls) {
            if (ath_tls_write(tls, ua, (size_t)m, req->timeout_ms) < 0) { resp->error = "Send failed"; dbgprintf("HTTP: send UA failed (TLS)\n"); ath_tls_close(tls); return -1; }
        } else
#endif
        {
            if (write_all(s, ua, (size_t)m) != 0) { if (s>=0) close(s); resp->error = "Send failed"; dbgprintf("HTTP: send UA failed\n"); return -1; }
        }
    }
    // Default Content-Type for JSON when POST
    if (req->method==ATH_HTTP_POST) {
        const char *ctype = "Content-Type: application/json\r\n";
        size_t ctlen = strlen(ctype);
#ifdef ATHENA_HAS_BEARSSL
        if (tls) {
            if (ath_tls_write(tls, ctype, ctlen, req->timeout_ms) < 0) { resp->error = "Send failed"; dbgprintf("HTTP: send content-type failed (TLS)\n"); ath_tls_close(tls); return -1; }
        } else
#endif
        {
            if (write_all(s, ctype, ctlen) != 0) { if (s>=0) close(s); resp->error = "Send failed"; dbgprintf("HTTP: send content-type failed\n"); return -1; }
        }
    }
    // Custom headers
    for (int i=0;i<req->headers_len && i<16;i++){
        const char *h = req->headers[i]; if (!h) continue;
        size_t hlen = strlen(h);
#ifdef ATHENA_HAS_BEARSSL
        if (tls) {
            if (ath_tls_write(tls, h, hlen, req->timeout_ms) < 0 || ath_tls_write(tls, "\r\n", 2, req->timeout_ms) < 0) { resp->error = "Send failed"; dbgprintf("HTTP: send custom header failed (TLS)\n"); ath_tls_close(tls); return -1; }
        } else
#endif
        {
            if (write_all(s, h, hlen) != 0 || write_all(s, "\r\n", 2) != 0) { if (s>=0) close(s); resp->error = "Send failed"; dbgprintf("HTTP: send custom header failed\n"); return -1; }
        }
    }
    // Authorization (Basic)
    if (req->userpwd && req->userpwd[0]) {
#ifdef ATHENA_HAS_BEARSSL
        if (tls) {
            if (send_basic_auth_tls(tls, req->userpwd, req->timeout_ms) != 0) { ath_tls_close(tls); resp->error = "Auth failed"; dbgprintf("HTTP: auth header failed (TLS)\n"); return -1; }
        } else
#endif
        {
            if (send_basic_auth(s, req->userpwd) != 0) { close(s); resp->error = "Auth failed"; dbgprintf("HTTP: auth header failed\n"); return -1; }
        }
    }
    // Content-Length + blank line
    size_t post_len = (req->method==ATH_HTTP_POST && req->postdata) ? strlen(req->postdata) : 0;
    char cl[64];
    if (req->method==ATH_HTTP_POST) {
        int m = snprintf(cl, sizeof(cl), "Content-Length: %u\r\n", (unsigned)post_len);
        if (m<0 || write_all(s, cl, (size_t)m) != 0) { close(s); resp->error = "Send failed"; dbgprintf("HTTP: send content-length failed\n"); return -1; }
    }
 #ifdef ATHENA_HAS_BEARSSL
    if (tls) {
        if (ath_tls_write(tls, "\r\n", 2, req->timeout_ms) < 0) { resp->error = "Send failed"; dbgprintf("HTTP: end headers failed (TLS)\n"); ath_tls_close(tls); return -1; }
    } else
#endif
    {
        if (write_all(s, "\r\n", 2) != 0) { if (s>=0) close(s); resp->error = "Send failed"; dbgprintf("HTTP: end headers failed\n"); return -1; }
    }
    dbgprintf("HTTP: headers finished\n");

    // POST body
    if (post_len) {
 #ifdef ATHENA_HAS_BEARSSL
        if (tls) {
            if (ath_tls_write(tls, req->postdata, post_len, req->timeout_ms) < 0) { resp->error = "Send failed"; dbgprintf("HTTP: send body failed (TLS)\n"); ath_tls_close(tls); return -1; }
        } else
#endif
        {
            if (write_all(s, req->postdata, post_len) != 0) { if (s>=0) close(s); resp->error = "Send failed"; dbgprintf("HTTP: send body failed\n"); return -1; }
        }
        dbgprintf("HTTP: sent POST body len=%u\n", (unsigned)post_len);
    }

    // Prepare connection reader (plain socket only)
    ath_http_conn_t conn = {0};
    conn.sock = s; conn.len = 0; conn.off = 0; conn.timeout_ms = req->timeout_ms;

    // Read status line
    char line[1024];
    // Read status line
#ifdef ATHENA_HAS_BEARSSL
    if (tls) {
        // Simple TLS line reader: read bytes one by one until \n
        size_t i = 0; int r;
        while (i + 1 < sizeof(line)) {
            r = ath_tls_read(tls, &line[i], 1, req->timeout_ms);
            if (r <= 0) { ath_tls_close(tls); resp->error = "No response"; dbgprintf("HTTP: no response line (TLS)\n"); return -1; }
            if (line[i] == '\n') { i++; break; }
            i++;
        }
        line[i] = '\0';
    } else
#endif
    {
        if (conn_readline(&conn, line, sizeof(line)) <= 0) { if (s>=0) close(s); resp->error = "No response"; dbgprintf("HTTP: no response line\n"); return -1; }
    }
    // Expect: HTTP/1.1 200 OK
    int code = 0; {
        char *p = strchr(line, ' ');
        if (p) code = atoi(p+1);
    }
    resp->status_code = code;
    dbgprintf("HTTP: status code %d\n", code);

    // Headers
    int is_chunked = 0; long content_length = -1; static char s_location[512]; s_location[0] = '\0';
    while (1) {
        int r;
#ifdef ATHENA_HAS_BEARSSL
        if (tls) {
            size_t i = 0; int rr;
            while (i + 1 < sizeof(line)) {
                rr = ath_tls_read(tls, &line[i], 1, req->timeout_ms);
                if (rr <= 0) { r = -1; break; }
                if (line[i] == '\n') { i++; break; }
                i++;
            }
            line[i] = '\0';
            r = (int)i;
        } else
#endif
        {
            r = conn_readline(&conn, line, sizeof(line));
        }
        if (r < 0) { close(s); resp->error = "Read header failed"; return -1; }
        if (r == 0) { close(s); resp->error = "Malformed headers"; return -1; }
        if (strcmp(line, "\r\n") == 0 || strcmp(line, "\n") == 0) break;
        trim_crlf(line);
        append_header(resp, line);
        if (!ci_strncmp(line, "Transfer-Encoding:", 18)) {
            const char *v = line + 18; while (*v==' '||*v=='\t') v++;
            // case-insensitive contains "chunked"
            const char *p = v;
            while (*p) {
                if ((to_lower((unsigned char)p[0])=='c') &&
                    (to_lower((unsigned char)p[1])=='h') &&
                    (to_lower((unsigned char)p[2])=='u') &&
                    (to_lower((unsigned char)p[3])=='n') &&
                    (to_lower((unsigned char)p[4])=='k') &&
                    (to_lower((unsigned char)p[5])=='e') &&
                    (to_lower((unsigned char)p[6])=='d')) { is_chunked = 1; break; }
                p++;
            }
        }
        if (!ci_strncmp(line, "Content-Length:", 15)) {
            const char *v = line + 15;
            while (*v == ' ' || *v == '\t' || *v == ':') v++;
            content_length = atol(v);
        }
        if (!ci_strncmp(line, "Location:", 9)) {
            const char *v = line + 9; while (*v==' '||*v=='\t') v++;
            strncpy(s_location, v, sizeof(s_location)-1); s_location[sizeof(s_location)-1]='\0';
        }
    }
    dbgprintf("HTTP: headers parsed (chunked=%d, content_length=%ld)\n", is_chunked, content_length);

    // Body
    resp->bytes_received = 0;
    int rc = 0;
    if (req->method != ATH_HTTP_HEAD) {
        if (is_chunked) {
            // chunked decoding
            while (1) {
                int rr;
#ifdef ATHENA_HAS_BEARSSL
                if (tls) {
                    size_t i = 0; int r1;
                    while (i + 1 < sizeof(line)) {
                        r1 = ath_tls_read(tls, &line[i], 1, req->timeout_ms);
                        if (r1 <= 0) { rr=-1; break; }
                        if (line[i] == '\n') { i++; break; }
                        i++;
                    }
                    line[i] = '\0';
                    rr = (int)i;
                } else
#endif
                {
                    rr = conn_readline(&conn, line, sizeof(line));
                }
                if (rr <= 0) { rc=-1; break; }
                trim_crlf(line);
                int chunk = hex_to_int(line);
                dbgprintf("HTTP: chunk size %d\n", chunk);
                if (chunk <= 0) {
                    // consume trailing CRLF and optional trailer headers
                    // read until blank line
                    do {
#ifdef ATHENA_HAS_BEARSSL
                        if (tls) {
                            size_t i2 = 0; int r2;
                            while (i2 + 1 < sizeof(line)) {
                                r2 = ath_tls_read(tls, &line[i2], 1, req->timeout_ms);
                                if (r2 <= 0) { rr=-1; break; }
                                if (line[i2] == '\n') { i2++; break; }
                                i2++;
                            }
                            line[i2] = '\0';
                            rr = (int)i2;
                        } else
#endif
                        {
                            rr = conn_readline(&conn, line, sizeof(line));
                        }
                        if (rr <= 0) break;
                    } while (!(strcmp(line,"\r\n")==0||strcmp(line,"\n")==0));
                    break;
                }
                size_t left = (size_t)chunk;
                uint8_t buf[1024];
                while (left > 0) {
                    size_t toread = left < sizeof(buf) ? left : sizeof(buf);
                    int rrd;
#ifdef ATHENA_HAS_BEARSSL
                    if (tls) {
                        rrd = ath_tls_read(tls, buf, toread, req->timeout_ms);
                    } else
#endif
                    {
                        rrd = conn_read(&conn, buf, toread);
                    }
                    if (rrd <= 0) { rc=-1; break; }
                    size_t got = (size_t)rrd;
                    if (req->on_data && req->on_data(buf, got, req->on_data_user) != 0) { rc=-1; break; }
                    resp->bytes_received += got;
                    left -= got;
                }
                // consume CRLF after chunk
                int rrc;
#ifdef ATHENA_HAS_BEARSSL
                if (tls) {
                    rrc = ath_tls_read(tls, buf, 2, req->timeout_ms);
                } else
#endif
                {
                    rrc = conn_read(&conn, buf, 2);
                }
                if (rrc <= 0) { rc=-1; break; }
                if (rc != 0) break;
            }
        } else if (content_length >= 0) {
            size_t left = (size_t)content_length;
            uint8_t buf[1024];
            while (left > 0) {
                size_t toread = left < sizeof(buf) ? left : sizeof(buf);
                int rre;
#ifdef ATHENA_HAS_BEARSSL
                if (tls) {
                    rre = ath_tls_read(tls, buf, toread, req->timeout_ms);
                } else
#endif
                {
                    rre = conn_read(&conn, buf, toread);
                }
                if (rre <= 0) { rc=-1; break; }
                size_t got = (size_t)rre;
                if (req->on_data && req->on_data(buf, got, req->on_data_user) != 0) { rc=-1; break; }
                resp->bytes_received += got;
                left -= got;
            }
        } else {
            // read until EOF
            uint8_t buf[1024];
            while (1) {
                // consume what's in buffer first
                if (!tls && conn.off < conn.len) {
                    size_t avail = conn.len - conn.off;
                    size_t toread = avail < sizeof(buf) ? avail : sizeof(buf);
                    memcpy(buf, conn.buf + conn.off, toread);
                    conn.off += toread;
                    if (req->on_data && req->on_data(buf, toread, req->on_data_user) != 0) { rc=-1; break; }
                    resp->bytes_received += toread;
                    continue;
                }
                int rrf;
#ifdef ATHENA_HAS_BEARSSL
                if (tls) {
                    rrf = ath_tls_read(tls, buf, sizeof(buf), req->timeout_ms);
                } else
#endif
                {
                    if (!wait_readable(conn.sock, conn.timeout_ms)) { rc=-1; break; }
                    ssize_t rr2 = recv(conn.sock, buf, sizeof(buf), 0);
                    rrf = (int)rr2;
                }
                if (rrf <= 0) break;
                if (req->on_data && req->on_data(buf, (size_t)rrf, req->on_data_user) != 0) { rc=-1; break; }
                resp->bytes_received += (size_t)rrf;
            }
        }
    }

    if (tls) { ath_tls_close(tls); } else if (s>=0) { close(s); }

    // Handle redirects (caller controls follow_redirects count)
    if ((code==301||code==302||code==303||code==307||code==308) && req->follow_redirects && s_location[0]) {
        // Simple: place location into resp->error as hint; actual loop handled by wrapper
        resp->error = s_location;
        dbgprintf("HTTP: redirect to %s\n", s_location);
        return 1; // signal redirect
    }

    dbgprintf("HTTP: completed, bytes=%u\n", (unsigned)resp->bytes_received);
    return rc;
}

int ath_http_perform(const ath_http_request_t *req, ath_http_response_t *resp) {
    if (!req || !resp) return -1;
    if (resp->headers) { free(resp->headers); resp->headers = NULL; }
    resp->status_code = 0; resp->bytes_received = 0; resp->error = NULL;
    int redirects_left = ATH_HTTP_MAX_REDIRECTS;
    char urlbuf[768];
    const char *cur_url = req->url;

    ath_http_request_t tmp = *req; // shallow copy; we'll mutate url and method for redirect handling if needed
    if (tmp.timeout_ms <= 0) tmp.timeout_ms = 5000;

    while (1) {
        tmp.url = cur_url;
        int rc = perform_once(&tmp, resp);
        if (rc == 1 && tmp.follow_redirects && redirects_left-- > 0 && resp->error) {
            // rc==1 means redirect; resp->error contains new URL (Location)
            strncpy(urlbuf, resp->error, sizeof(urlbuf)-1); urlbuf[sizeof(urlbuf)-1]='\0';
            resp->error = NULL;
            // Per HTTP spec, 303 -> GET
            if (resp->status_code == 303) tmp.method = ATH_HTTP_GET;
            cur_url = urlbuf;
            continue;
        }
        return rc;
    }
}
