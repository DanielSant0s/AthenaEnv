#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <athena/request.h>
#include <ath_net.h>
#include <network.h>
#include <taskman.h>
#include <dbgprintf.h>

static int athena_request_on_data_mem(const uint8_t *data, size_t len, void *user)
{
    AthenaRequestChunk *chunk = user;

    chunk->memory = realloc(chunk->memory, chunk->size + len + 1);
    if (!chunk->memory)
        return 0;

    memcpy(&(chunk->memory[chunk->size]), data, len);
    chunk->size += len;
    chunk->memory[chunk->size] = 0;
    chunk->timer = clock();
    chunk->transferring = true;
    return 0;
}

static int athena_request_on_data_file(const uint8_t *data, size_t len, void *user)
{
    AthenaRequestChunk *chunk = user;
    size_t written = fwrite(data, 1, len, chunk->fp);

    chunk->size += written;
    chunk->timer = clock();
    chunk->transferring = true;
    return 0;
}

static void athena_request_thread(void *data)
{
    AthenaRequest *req = data;
    req->chunk.timer = clock();

    ath_http_request_t http_req = {0};
    ath_http_response_t http_resp = {0};

    http_req.url = req->url;
    http_req.method = req->method;
    http_req.useragent = req->useragent;
    http_req.userpwd = req->userpwd;
    http_req.postdata = req->postdata;
    for (int i = 0; i < req->headers_len && i < 16; ++i)
        http_req.headers[i] = req->headers[i];
    http_req.headers_len = req->headers_len;
    http_req.follow_redirects = req->follow_redirects;
    http_req.keepalive = req->keepalive;
    http_req.verify_tls = req->verify_tls;
    http_req.timeout_ms = req->timeout_ms;
    http_req.on_data = req->save ? athena_request_on_data_file : athena_request_on_data_mem;
    http_req.on_data_user = &req->chunk;

    if (ath_http_perform(&http_req, &http_resp) != 0) {
        req->error = http_resp.error ? http_resp.error : "Network error";
        dbgprintf("%s\n", req->error);
    }

    req->response_code = http_resp.status_code;
    if (req->tid == -1) {
        req->response_headers = http_resp.headers;
    } else if (http_resp.headers) {
        free(http_resp.headers);
    }

    if (req->save && req->chunk.fp) {
        fclose(req->chunk.fp);
        req->chunk.fp = NULL;
    }

    req->ready = true;
}

AthenaRequest *athena_request_create(void)
{
    AthenaRequest *req = calloc(1, sizeof(*req));
    if (!req)
        return NULL;

    req->keepalive = 0L;
    req->timeout_ms = 5000;
    req->verify_tls = 1;
    req->follow_redirects = 1;
    req->useragent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36";
    req->tid = -1;
    return req;
}

void athena_request_destroy(AthenaRequest *req)
{
    free(req);
}

void athena_request_set_keepalive(AthenaRequest *req, bool keepalive)
{
    if (req)
        req->keepalive = keepalive ? 1L : 0L;
}

void athena_request_set_useragent(AthenaRequest *req, const char *useragent)
{
    if (req)
        req->useragent = useragent;
}

void athena_request_set_userpwd(AthenaRequest *req, const char *userpwd)
{
    if (req)
        req->userpwd = userpwd;
}

void athena_request_set_timeout(AthenaRequest *req, int timeout_ms)
{
    if (req)
        req->timeout_ms = timeout_ms;
}

void athena_request_set_verify_tls(AthenaRequest *req, bool verify)
{
    if (req)
        req->verify_tls = verify ? 1 : 0;
}

void athena_request_set_follow_redirects(AthenaRequest *req, bool follow)
{
    if (req)
        req->follow_redirects = follow ? 1 : 0;
}

int athena_request_set_headers(AthenaRequest *req, const char **headers, int count)
{
    if (!req)
        return -1;
    if (count > 16)
        return -1;

    req->headers_len = count;
    for (int i = 0; i < count; i++)
        req->headers[i] = (char *)headers[i];
    return 0;
}

void athena_request_run(AthenaRequest *req)
{
    if (!req)
        return;
    req->error = NULL;
    req->ready = false;
    req->tid = -1;
    athena_request_thread(req);
}

int athena_request_run_async(AthenaRequest *req, const char *task_name)
{
    if (!req)
        return -1;

    req->error = NULL;
    req->ready = false;
    req->tid = create_task(task_name, athena_request_thread, 4096 * 10, 16);
    if (req->tid < 0)
        return -1;

    init_task(req->tid, req);
    return 0;
}

bool athena_request_poll(AthenaRequest *req, int timeout_ms, int transfer_timeout_ms)
{
    if (!req)
        return false;

    /* Only the worker thread closes the save FILE* and sets ready.
     * Forcing ready here raced with fwrite() and double-closed the FILE,
     * which corrupted malloc metadata (TLB miss inside unlink). */
    if (timeout_ms >= 0 && transfer_timeout_ms >= 0 && !req->ready && !req->error) {
        if ((clock() - req->chunk.timer) / 1000 > transfer_timeout_ms &&
            !req->chunk.transferring && req->chunk.timer > 0)
            req->error = "Network: Asynchronous operation timeout.\n";
    }

    if (req->save && (req->ready || req->error)) {
        req->url = NULL;
        req->chunk.memory = NULL;
        req->chunk.fp = NULL;
        req->chunk.size = 0;
        req->chunk.timer = 0;
        req->chunk.transferring = false;
        if (req->tid >= 0)
            kill_task(req->tid);
    }

    return req->ready;
}

AthenaRequestResponse *athena_request_take_response(AthenaRequest *req)
{
    AthenaRequestResponse *resp;

    if (!req || (!req->ready && !req->error))
        return NULL;

    resp = calloc(1, sizeof(*resp));
    if (!resp)
        return NULL;

    resp->body = req->chunk.memory;
    resp->body_size = req->chunk.size;
    resp->status_code = req->response_code;
    resp->headers = req->response_headers;

    if (req->tid >= 0)
        kill_task(req->tid);

    req->url = NULL;
    req->chunk.memory = NULL;
    req->chunk.fp = NULL;
    req->chunk.size = 0;
    req->chunk.timer = 0;
    req->chunk.transferring = false;
    req->postdata = NULL;
    req->response_headers = NULL;

    return resp;
}

void athena_request_response_free(AthenaRequestResponse *resp)
{
    if (!resp)
        return;
    free(resp->body);
    free(resp->headers);
    free(resp);
}

static int athena_request_prepare(AthenaRequest *req, const char *url, int method, bool save)
{
    if (!req || !url)
        return -1;

    req->error = NULL;
    req->ready = false;
    req->url = url;
    req->method = method;
    req->save = save;
    req->chunk.timer = 0;
    req->chunk.transferring = false;
    req->tid = -1;
    return 0;
}

int athena_request_get(AthenaRequest *req, const char *url, AthenaRequestResponse **out)
{
    if (athena_request_prepare(req, url, ATHENA_REQUEST_GET, false) != 0)
        return -1;

    req->chunk.memory = malloc(1);
    req->chunk.size = 0;
    athena_request_run(req);

    if (req->error)
        return -1;

    if (out)
        *out = athena_request_take_response(req);
    return 0;
}

int athena_request_head(AthenaRequest *req, const char *url, AthenaRequestResponse **out)
{
    if (athena_request_prepare(req, url, ATHENA_REQUEST_HEAD, false) != 0)
        return -1;

    req->chunk.memory = malloc(1);
    req->chunk.size = 0;
    athena_request_run(req);

    if (req->error)
        return -1;

    if (out)
        *out = athena_request_take_response(req);
    return 0;
}

int athena_request_post(AthenaRequest *req, const char *url, const char *body, AthenaRequestResponse **out)
{
    if (athena_request_prepare(req, url, ATHENA_REQUEST_POST, false) != 0)
        return -1;

    req->postdata = body;
    req->chunk.memory = malloc(1);
    req->chunk.size = 0;
    athena_request_run(req);

    if (req->error)
        return -1;

    if (out)
        *out = athena_request_take_response(req);
    return 0;
}

int athena_request_download(AthenaRequest *req, const char *url, const char *path)
{
    if (athena_request_prepare(req, url, ATHENA_REQUEST_GET, true) != 0)
        return -1;

    req->chunk.fp = fopen(path, "wb");
    if (!req->chunk.fp)
        return -1;

    athena_request_run(req);
    return req->error ? -1 : 0;
}
