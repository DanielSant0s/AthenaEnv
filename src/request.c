
#include <network.h>
#include <ath_net.h>

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

// Adapters from native backend on_data to existing callbacks
static int AthenaOnDataMem(const uint8_t *data, size_t len, void *user) {
  struct MemoryStruct *ms = (struct MemoryStruct*)user;
  size_t r = AsyncWriteMemoryCallback((void*)data, 1, len, ms);
  return (r == len) ? 0 : -1;
}

static int AthenaOnDataFile(const uint8_t *data, size_t len, void *user) {
  struct MemoryStruct *ms = (struct MemoryStruct*)user;
  size_t r = AsyncWriteFileCallback((void*)data, 1, len, ms);
  return (r == len) ? 0 : -1;
}

void requestThread(void* data) {
    JSRequestData *s = data;
    s->chunk.timer = clock();

    ath_http_request_t req = {0};
    ath_http_response_t resp = {0};

    req.url = s->url;
    req.method = (s->method == ATHENA_GET) ? ATH_HTTP_GET :
                 (s->method == ATHENA_POST) ? ATH_HTTP_POST : ATH_HTTP_HEAD;
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
