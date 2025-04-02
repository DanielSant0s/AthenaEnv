
#include <network.h>

CURL* curl = NULL;

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

void requestThread(void* data) {
    CURLcode res;

    JSRequestData *s = data;

    curl_easy_reset(curl);

    struct curl_slist *header_chunk = NULL;

    curl_easy_setopt(curl, CURLOPT_URL, s->url);

    for(int i = 0; i < s->headers_len; i++) {
        header_chunk = curl_slist_append(header_chunk, s->headers[i]);
    }

	header_chunk = curl_slist_append(header_chunk, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_chunk);

	switch (s->method){
	case ATHENA_GET:
		curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
		break;
	case ATHENA_POST:
		curl_easy_setopt(curl, CURLOPT_POST, 1L);
		break;
	case ATHENA_HEAD:
		curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
		curl_easy_setopt(curl, CURLOPT_HEADER, 1L); 
		break;
	default:
		break;
	}

	if (s->postdata){
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, s->postdata);
	}

    if(s->save) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AsyncWriteFileCallback);
    } else {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AsyncWriteMemoryCallback);
    }

    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&(s->chunk));

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

    curl_easy_setopt(curl, CURLOPT_USERPWD, s->userpwd);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, s->useragent);

    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, s->keepalive);

    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

	curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);

    s->chunk.timer = clock();
    res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        s->error = "Error while downloading file\n"; //, curl_easy_strerror(res));
    }

    if(s->save) fclose(s->chunk.fp);

    s->ready = true;

    //exitkill_task();
}