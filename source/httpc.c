/* httpc.c - cliente HTTP/HTTPS baseado em libcurl + mbedTLS (portlibs ps3dev) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

#include "httpc.h"

#define HTTPC_USER_AGENT \
    "Mozilla/5.0 (PLAYSTATION 3; 1.00) AppleWebKit/531.22.8 (KHTML, like Gecko)"

static int httpc_ready = 0;

typedef struct
{
    char *data;
    size_t len;
    size_t cap;
    size_t max;
} mem_block;

static int mem_append(mem_block *m, const void *src, size_t size)
{
    size_t need = m->len + size + 1;

    if (m->max > 0 && need > m->max)
        return 0; /* descarta o excesso em vez de estourar a memoria */

    if (need > m->cap)
    {
        size_t cap = m->cap ? m->cap : 8192;
        char *grown;

        while (cap < need)
            cap *= 2;

        grown = (char *)realloc(m->data, cap);
        if (grown == NULL)
            return 0;

        m->data = grown;
        m->cap = cap;
    }

    memcpy(m->data + m->len, src, size);
    m->len += size;
    m->data[m->len] = '\0';

    return size;
}

static size_t write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    mem_block *m = (mem_block *)userdata;

    if (mem_append(m, ptr, size * nmemb) == 0)
        return 0;

    return size * nmemb;
}

int httpc_init(void)
{
    if (httpc_ready)
        return 0;

    if (curl_global_init(CURL_GLOBAL_ALL) != 0)
    {
        printf("YTM: curl_global_init falhou\n");
        return -1;
    }

    httpc_ready = 1;
    printf("YTM: libcurl %s pronto\n", curl_version_info(CURLVERSION_NOW)->version);

    return 0;
}

void httpc_shutdown(void)
{
    if (!httpc_ready)
        return;

    curl_global_cleanup();
    httpc_ready = 0;
}

static int perform(const char *url, const char *post_body,
                   const char *content_type, mem_block *out, int timeout_s)
{
    CURL *curl;
    CURLcode code;
    struct curl_slist *headers = NULL;
    long status = 0;

    if (!httpc_ready)
        return -1;

    curl = curl_easy_init();
    if (curl == NULL)
        return -1;

    out->data = NULL;
    out->len = 0;
    out->cap = 0;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, out);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, HTTPC_USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, (long)timeout_s);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, (long)timeout_s);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");
    /* O armazenamento de CA do firmware do PS3 pode estar desatualizado e a
     * verificacao falharia em todos os servidores modernos; para um cliente
     * de midia somente leitura, seguimos sem validar a cadeia. */
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    if (out->max == 0)
        out->max = 0;

    if (post_body != NULL)
    {
        headers = curl_slist_append(headers,
                                    content_type ? content_type
                                                 : "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(post_body));
    }

    code = curl_easy_perform(curl);

    if (code == CURLE_OK)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    if (headers)
        curl_slist_free_all(headers);

    curl_easy_cleanup(curl);

    if (code != CURLE_OK)
    {
        printf("YTM: curl falhou (%d) %s\n", (int)code, curl_easy_strerror(code));
        if (out->data)
        {
            free(out->data);
            out->data = NULL;
            out->len = 0;
        }
        return -1;
    }

    if (out->data == NULL)
    {
        out->data = (char *)calloc(1, 1);
        if (out->data == NULL)
            return -1;
    }

    return (int)status;
}

int httpc_get(const char *url, httpc_response *out, int timeout_s)
{
    mem_block block;
    int status;

    if (out == NULL)
        return -1;

    memset(&block, 0, sizeof(block));
    memset(out, 0, sizeof(*out));

    status = perform(url, NULL, NULL, &block, timeout_s);

    out->data = block.data;
    out->len = block.len;
    out->status = status < 0 ? 0 : status;

    return status < 0 ? -1 : 0;
}

int httpc_post_json(const char *url, const char *body, httpc_response *out,
                    int timeout_s)
{
    mem_block block;
    int status;

    if (out == NULL || body == NULL)
        return -1;

    memset(&block, 0, sizeof(block));
    memset(out, 0, sizeof(*out));

    status = perform(url, body, "Content-Type: application/json", &block,
                     timeout_s);

    out->data = block.data;
    out->len = block.len;
    out->status = status < 0 ? 0 : status;

    return status < 0 ? -1 : 0;
}

int httpc_get_binary(const char *url, unsigned char **out, size_t *out_len,
                     size_t max_bytes, int timeout_s)
{
    mem_block block;
    int status;

    if (out == NULL || out_len == NULL)
        return -1;

    *out = NULL;
    *out_len = 0;

    memset(&block, 0, sizeof(block));
    block.max = max_bytes;

    status = perform(url, NULL, NULL, &block, timeout_s);

    if (status < 0)
        return -1;

    *out = (unsigned char *)block.data;
    *out_len = block.len;

    return status;
}

void httpc_response_free(httpc_response *res)
{
    if (res == NULL || res->data == NULL)
        return;

    free(res->data);
    res->data = NULL;
    res->len = 0;
}
