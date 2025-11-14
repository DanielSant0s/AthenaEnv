// AthenaEnv native networking backend (HTTP/TLS/WebSocket) interfaces
#ifndef ATHENA_NATIVE_NET_H
#define ATHENA_NATIVE_NET_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

// This header must not depend on JS or higher layers.

#ifdef __cplusplus
extern "C" {
#endif

// Opaque types for native TLS and WebSocket
typedef struct ath_tls_ctx ath_tls_ctx_t;
typedef struct ath_ws_ctx ath_ws_ctx_t;

// -------- HTTP --------
typedef enum {
  ATH_HTTP_GET = 0,
  ATH_HTTP_POST,
  ATH_HTTP_HEAD,
} ath_http_method_t;

typedef struct {
  ath_http_method_t method;
  const char *url;
  const char *useragent;  // optional
  const char *userpwd;    // optional
  const char *postdata;   // for POST
  const char *headers[16];
  int headers_len;
  int follow_redirects;   // 0/1
  long keepalive;         // 0/1
  int verify_tls;         // 0/1
  int timeout_ms;         // connect/read timeout (best-effort)

  // Streaming sink: called for each received data chunk
  int (*on_data)(const uint8_t *data, size_t len, void *user);
  void *on_data_user;
} ath_http_request_t;

typedef struct {
  long status_code;       // HTTP status code
  size_t bytes_received;  // total bytes
  const char *error;      // optional error string (static or owned by impl)
  char *headers;          // raw response headers (one per line, \n-separated)
} ath_http_response_t;

// Perform an HTTP request. Returns 0 on success.
int ath_http_perform(const ath_http_request_t *req, ath_http_response_t *resp);

// Minimal TLS helpers (stubs for now)
ath_tls_ctx_t *ath_tls_connect(const char *host, uint16_t port, bool verify, bool keepalive);
int ath_tls_read(ath_tls_ctx_t *ctx, void *buf, size_t len, int timeout_ms);
int ath_tls_write(ath_tls_ctx_t *ctx, const void *buf, size_t len, int timeout_ms);
void ath_tls_close(ath_tls_ctx_t *ctx);

// WebSocket helpers
ath_ws_ctx_t *ath_ws_connect(const char *url, bool verify_tls);
int ath_ws_recv(ath_ws_ctx_t *ws, uint8_t *buf, size_t buflen, size_t *out_len);
int ath_ws_send(ath_ws_ctx_t *ws, const uint8_t *buf, size_t len, int is_binary);
void ath_ws_close(ath_ws_ctx_t *ws);

#ifdef __cplusplus
}
#endif

#endif // ATHENA_NATIVE_NET_H
