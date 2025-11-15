
#include <network.h>
#include <ath_net.h>

static int AthenaOnDataMem(const uint8_t *data, size_t len, void *user) {
  struct MemoryStruct *ms = (struct MemoryStruct*)user;

  ms->memory = realloc(ms->memory, ms->size + len + 1);
  if(ms->memory == NULL) {
    /* out of memory */
    dbgprintf("not enough memory (realloc returned NULL)\n");
    return 0;
  }

  memcpy(&(ms->memory[ms->size]), data, len);
  ms->size += len;
  ms->memory[ms->size] = 0;

  ms->timer = clock();
  ms->transferring = true;
  
  return 0;
}

static int AthenaOnDataFile(const uint8_t *data, size_t len, void *user) {
  struct MemoryStruct *ms = (struct MemoryStruct*)user;

  size_t written = fwrite(data, 1, len, ms->fp);

  ms->size += written;
  ms->timer = clock();
  ms->transferring = true;

  return 0;
}

void requestThread(void* data) {
    JSRequestData *s = data;
    s->chunk.timer = clock();

    ath_http_request_t req = {0};
    ath_http_response_t resp = {0};

    req.url = s->url;
    req.method = s->method;
    req.useragent = s->useragent;
    req.userpwd = s->userpwd;
    req.postdata = s->postdata;
    for (int i = 0; i < s->headers_len && i < 16; ++i) req.headers[i] = s->headers[i];
    req.headers_len = s->headers_len;
    req.follow_redirects = s->follow_redirects;
    req.keepalive = s->keepalive;
    req.verify_tls = s->verify_tls;
    req.timeout_ms = s->timeout_ms;
    if (s->save) req.on_data = AthenaOnDataFile; else req.on_data = AthenaOnDataMem;
    req.on_data_user = &(s->chunk);

    if (ath_http_perform(&req, &resp) != 0) {
        s->error = resp.error ? resp.error : "Network error";
        printf("%s\n", s->error);
    }
    s->response_code = resp.status_code;
    if (s->tid == -1) {
        s->response_headers = resp.headers;
    } else {
        if (resp.headers) free(resp.headers);
    }
    if (s->save && s->chunk.fp) fclose(s->chunk.fp);
    s->ready = true;
}
