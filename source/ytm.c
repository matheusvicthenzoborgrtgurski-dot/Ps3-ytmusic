/* ytm.c - cliente do YouTube Music (InnerTube WEB_REMIX) e do proxy local */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sysmodule/sysmodule.h>
#include <net/net.h>
#include <json-c/json.h>

#include "ytm.h"
#include "httpc.h"
#include "audio.h"

#define INNERTUBE_SEARCH "https://music.youtube.com/youtubei/v1/search?prettyPrint=false"
#define INNERTUBE_PLAYER "https://www.youtube.com/youtubei/v1/player?prettyPrint=false"
#define CLIENT_VERSION   "1.20260101.01.00"
#define MAX_MP3_BYTES    (14u * 1024u * 1024u)

#define CONFIG_PATH "/dev_hdd0/tmp/ytm-proxy.txt"

static int net_ok = 0;
static int modules_loaded = 0;
static char proxy_url[256] = "";

static void load_config(void)
{
    FILE *fp = fopen(CONFIG_PATH, "r");
    char line[200];

    proxy_url[0] = '\0';

    if (fp == NULL)
        return;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        size_t len = strlen(line);

        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' ||
                           line[len - 1] == ' '))
            line[--len] = '\0';

        if (strncmp(line, "proxy=", 6) == 0)
        {
            snprintf(proxy_url, sizeof(proxy_url), "%s", line + 6);
            break;
        }
    }

    fclose(fp);

    if (proxy_url[0] != '\0')
        printf("YTM: proxy configurado em %s\n", proxy_url);
}

int ytm_init(void)
{
    if (sysModuleLoad(SYSMODULE_NET) < 0)
    {
        printf("YTM: SYSMODULE_NET indisponivel\n");
        return -1;
    }

    modules_loaded = 1;

    if (netInitialize() < 0)
    {
        printf("YTM: netInitialize falhou\n");
        return -1;
    }

    net_ok = 1;

    load_config();

    if (httpc_init() != 0)
        return -1;

    return 0;
}

void ytm_shutdown(void)
{
    httpc_shutdown();

    if (net_ok)
    {
        netDeinitialize();
        net_ok = 0;
    }

    if (modules_loaded)
    {
        sysModuleUnload(SYSMODULE_NET);
        modules_loaded = 0;
    }
}

int ytm_net_ok(void) { return net_ok; }
const char *ytm_proxy_url(void) { return proxy_url; }

/* ------------------------------------------------------------------ */
/* utilidades de texto                                                */
/* ------------------------------------------------------------------ */

static void json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t o = 0;

    for (; *src && o + 2 < dst_size; src++)
    {
        unsigned char c = (unsigned char)*src;

        if (c == '"' || c == '\\')
        {
            dst[o++] = '\\';
            dst[o++] = (char)c;
        }
        else if (c == '\n')
        {
            dst[o++] = ' ';
        }
        else if (c < 0x20)
        {
            continue;
        }
        else
        {
            dst[o++] = (char)c;
        }
    }

    dst[o] = '\0';
}

static void url_escape(const char *src, char *dst, size_t dst_size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t o = 0;

    for (; *src && o + 4 < dst_size; src++)
    {
        unsigned char c = (unsigned char)*src;

        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')
        {
            dst[o++] = (char)c;
        }
        else
        {
            dst[o++] = '%';
            dst[o++] = hex[c >> 4];
            dst[o++] = hex[c & 0x0f];
        }
    }

    dst[o] = '\0';
}

static int parse_clock(const char *text)
{
    int parts[3] = { 0, 0, 0 };
    int count = 0;
    const char *p = text;

    while (*p && count < 3)
    {
        if (*p >= '0' && *p <= '9')
        {
            parts[count] = parts[count] * 10 + (*p - '0');
        }
        else if (*p == ':')
        {
            count++;
        }
        else
        {
            break;
        }
        p++;
    }

    if (count == 0)
        return parts[0] > 0 ? parts[0] : 0;
    if (count == 1)
        return parts[0] * 60 + parts[1];
    if (count == 2)
        return parts[0] * 3600 + parts[1] * 60 + parts[2];

    return 0;
}

/* text.runs[*].text concatenado */
static void join_runs(json_object *text_obj, char *out, size_t out_size)
{
    json_object *runs;
    int i, n;

    out[0] = '\0';

    if (text_obj == NULL ||
        !json_object_object_get_ex(text_obj, "runs", &runs))
        return;

    n = json_object_array_length(runs);

    for (i = 0; i < n; i++)
    {
        json_object *run = json_object_array_get_idx(runs, i);
        json_object *txt;
        size_t len;

        if (run == NULL || !json_object_object_get_ex(run, "text", &txt))
            continue;

        len = strlen(out);
        snprintf(out + len, out_size > len ? out_size - len : 0, "%s",
                 json_object_get_string(txt));
    }
}

static void first_run(json_object *text_obj, char *out, size_t out_size)
{
    out[0] = '\0';

    if (text_obj == NULL)
        return;

    join_runs(text_obj, out, out_size);
}

/* ------------------------------------------------------------------ */
/* InnerTube                                                          */
/* ------------------------------------------------------------------ */

static void build_context(char *buf, size_t size, const char *client)
{
    snprintf(buf, size,
             "{\"context\":{\"client\":{\"clientName\":\"%s\","
             "\"clientVersion\":\"%s\",\"hl\":\"pt\",\"gl\":\"BR\"}",
             client, CLIENT_VERSION);
}

/* Segunda coluna do InnerTube: "Musica . Daft Punk . 5:38" ou
 * "Album . Daft Punk . 2013". O separador e o bullet UTF-8 (e2 80 a2). */
static void parse_meta_column(const char *text, ytm_track *track)
{
    char buf[220];
    char *cursor;

    snprintf(buf, sizeof(buf), "%s", text);
    cursor = buf;

    while (cursor != NULL && *cursor != '\0')
    {
        char *next = strstr(cursor, "\xe2\x80\xa2");
        char *tok = cursor;
        char *end;

        if (next != NULL)
        {
            *next = '\0';
            next += 3;
        }

        while (*tok == ' ')
            tok++;

        end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\r'))
            *--end = '\0';

        if (*tok != '\0')
        {
            if (strstr(tok, "lbum") != NULL || strcmp(tok, "Album") == 0)
                track->kind = YTM_KIND_ALBUM;
            else if (strstr(tok, "Playlist") != NULL ||
                     strstr(tok, "lista") != NULL)
                track->kind = YTM_KIND_PLAYLIST;
            else if (strstr(tok, "rtista") != NULL ||
                     strcmp(tok, "Artist") == 0)
                track->kind = YTM_KIND_ARTIST;
            else if (strchr(tok, ':') != NULL)
                track->duration = parse_clock(tok);
            else if (strlen(tok) == 4 && tok[0] >= '1' && tok[0] <= '2')
            {
                /* ano de lancamento: ignora */
            }
            else if (tok[0] >= '0' && tok[0] <= '9' && strchr(tok, ' ') == NULL)
            {
                /* numero solto (faixa, ano): ignora */
            }
            else if (track->artist[0] == '\0')
                snprintf(track->artist, sizeof(track->artist), "%s", tok);
            else if (track->album[0] == '\0')
                snprintf(track->album, sizeof(track->album), "%s", tok);
        }

        cursor = next;
    }
}

static int parse_search_item(json_object *item, ytm_track *track)
{
    json_object *flex, *fixed, *playlist, *thumb, *obj;

    memset(track, 0, sizeof(*track));
    track->kind = YTM_KIND_SONG;

    /* videoId */
    if (json_object_object_get_ex(item, "playlistItemData", &playlist))
    {
        if (json_object_object_get_ex(playlist, "videoId", &obj))
            snprintf(track->id, sizeof(track->id), "%s",
                     json_object_get_string(obj));
    }

    if (track->id[0] == '\0' && json_object_object_get_ex(item, "videoId", &obj))
        snprintf(track->id, sizeof(track->id), "%s", json_object_get_string(obj));

    /* titulo e subtitulo */
    if (json_object_object_get_ex(item, "flexColumns", &flex))
    {
        int i, n = json_object_array_length(flex);

        for (i = 0; i < n && i < 2; i++)
        {
            json_object *col = json_object_array_get_idx(flex, i);
            json_object *renderer, *text;

            if (col == NULL ||
                !json_object_object_get_ex(
                    col, "musicResponsiveListItemFlexColumnRenderer", &renderer))
                continue;

            if (!json_object_object_get_ex(renderer, "text", &text))
                continue;

            if (i == 0)
            {
                first_run(text, track->title, sizeof(track->title));
            }
            else if (i == 1)
            {
                char meta[220];

                first_run(text, meta, sizeof(meta));
                parse_meta_column(meta, track);
            }
        }
    }

    if (track->kind == YTM_KIND_ALBUM && track->album[0] == '\0')
        snprintf(track->album, sizeof(track->album), "%s", track->title);

    /* duracao em fixedColumns (playlists, albuns e listas antigas) */
    if (track->duration == 0)
    {
        char duration_text[24] = "";
        json_object *col;

        if (json_object_object_get_ex(item, "fixedColumns", &col))
        {
            col = json_object_array_get_idx(col, 0);

            if (col != NULL &&
                json_object_object_get_ex(
                    col, "musicResponsiveListItemFixedColumnRenderer",
                    &fixed) &&
                json_object_object_get_ex(fixed, "text", &obj))
            {
                first_run(obj, duration_text, sizeof(duration_text));

                if (strchr(duration_text, ':') != NULL)
                    track->duration = parse_clock(duration_text);
            }
        }
    }

    /* capa */
    if (json_object_object_get_ex(item, "thumbnail", &thumb))
    {
        json_object *renderer, *thumbs;

        if (json_object_object_get_ex(thumb, "musicThumbnailRenderer", &renderer) &&
            json_object_object_get_ex(renderer, "thumbnail", &thumbs) &&
            json_object_object_get_ex(thumbs, "thumbnails", &thumbs))
        {
            int n = json_object_array_length(thumbs);

            if (n > 0)
            {
                json_object *last = json_object_array_get_idx(thumbs, n - 1);
                json_object *url;

                if (last != NULL && json_object_object_get_ex(last, "url", &url))
                    snprintf(track->art_url, sizeof(track->art_url), "%s",
                             json_object_get_string(url));
            }
        }
    }

    /* musicas precisam de videoId; albuns/playlists/artistas nao tem */
    if (track->title[0] == '\0')
        return 0;

    if (track->kind == YTM_KIND_SONG && track->id[0] == '\0')
        return 0;

    return 1;
}

static void walk_results(json_object *node, ytm_track *tracks, int max,
                         int *count, int depth)
{
    if (node == NULL || depth > 40 || *count >= max)
        return;

    if (json_object_get_type(node) == json_type_array)
    {
        int i, n = json_object_array_length(node);

        for (i = 0; i < n && *count < max; i++)
            walk_results(json_object_array_get_idx(node, i), tracks, max, count,
                         depth + 1);
        return;
    }

    if (json_object_get_type(node) != json_type_object)
        return;

    {
        json_object *item;

        if (json_object_object_get_ex(node, "musicResponsiveListItemRenderer",
                                     &item))
        {
            ytm_track t;

            if (parse_search_item(item, &t))
            {
                tracks[*count] = t;
                (*count)++;
            }
        }
    }

    {
        json_object_iter iter;

        json_object_object_foreachC(node, iter)
        {
            if (*count >= max)
                break;
            walk_results(iter.val, tracks, max, count, depth + 1);
        }
    }
}

static int search_innertube(const char *query, ytm_track *tracks, int max,
                            int *found, char *err, size_t err_size)
{
    char escaped[512];
    char body[1536];
    char context[512];
    httpc_response res;
    json_object *root;
    int count = 0;

    json_escape(query, escaped, sizeof(escaped));
    build_context(context, sizeof(context), "WEB_REMIX");
    snprintf(body, sizeof(body), "%s,\"query\":\"%s\"}}", context, escaped);

    *found = 0;

    if (httpc_post_json(INNERTUBE_SEARCH, body, &res, 25) != 0)
    {
        snprintf(err, err_size, "Sem resposta do YouTube Music (rede)");
        return -1;
    }

    if (res.data == NULL)
    {
        snprintf(err, err_size, "Resposta vazia (HTTP %ld)", res.status);
        return -1;
    }

    if (res.status != 200)
    {
        snprintf(err, err_size, "YouTube Music respondeu HTTP %ld", res.status);
        httpc_response_free(&res);
        return -1;
    }

    root = json_tokener_parse(res.data);
    httpc_response_free(&res);

    if (root == NULL)
    {
        snprintf(err, err_size, "JSON invalido na pesquisa");
        return -1;
    }

    walk_results(root, tracks, max, &count, 0);
    json_object_put(root);

    *found = count;

    if (count == 0)
        snprintf(err, err_size, "Nenhuma musica encontrada para \"%s\"", query);

    return 0;
}

/* ------------------------------------------------------------------ */
/* proxy local                                                        */
/* ------------------------------------------------------------------ */

static int search_proxy(const char *query, ytm_track *tracks, int max,
                        int *found, char *err, size_t err_size)
{
    char url[900];
    char esc[256];
    httpc_response res;
    json_object *root, *list;
    int i, count = 0;

    *found = 0;
    url_escape(query, esc, sizeof(esc));
    snprintf(url, sizeof(url), "%s/search?q=%s", proxy_url, esc);

    if (httpc_get(url, &res, 25) != 0 || res.data == NULL)
    {
        snprintf(err, err_size, "Proxy nao respondeu em %s", proxy_url);
        if (res.data)
            httpc_response_free(&res);
        return -1;
    }

    root = json_tokener_parse(res.data);
    httpc_response_free(&res);

    if (root == NULL)
    {
        snprintf(err, err_size, "JSON invalido do proxy");
        return -1;
    }

    if (!json_object_object_get_ex(root, "tracks", &list))
    {
        json_object_put(root);
        snprintf(err, err_size, "Proxy sem lista \"tracks\"");
        return -1;
    }

    for (i = 0; i < json_object_array_length(list) && count < max; i++)
    {
        json_object *item = json_object_array_get_idx(list, i);
        json_object *v;
        ytm_track *t = &tracks[count];

        memset(t, 0, sizeof(*t));

        if (json_object_object_get_ex(item, "id", &v))
            snprintf(t->id, sizeof(t->id), "%s", json_object_get_string(v));
        if (json_object_object_get_ex(item, "title", &v))
            snprintf(t->title, sizeof(t->title), "%s", json_object_get_string(v));
        if (json_object_object_get_ex(item, "artist", &v))
            snprintf(t->artist, sizeof(t->artist), "%s", json_object_get_string(v));
        if (json_object_object_get_ex(item, "album", &v))
            snprintf(t->album, sizeof(t->album), "%s", json_object_get_string(v));
        if (json_object_object_get_ex(item, "duration", &v))
            t->duration = json_object_get_int(v);
        if (json_object_object_get_ex(item, "art", &v))
            snprintf(t->art_url, sizeof(t->art_url), "%s",
                     json_object_get_string(v));
        if (json_object_object_get_ex(item, "stream", &v))
            snprintf(t->stream_url, sizeof(t->stream_url), "%s",
                     json_object_get_string(v));

        if (t->title[0] != '\0')
            count++;
    }

    json_object_put(root);
    *found = count;

    if (count == 0)
        snprintf(err, err_size, "Proxy nao achou nada para \"%s\"", query);

    return 0;
}

int ytm_search(const char *query, ytm_track *tracks, int max, int *found,
               char *err, size_t err_size)
{
    if (found)
        *found = 0;
    if (err && err_size)
        err[0] = '\0';

    if (!net_ok)
    {
        snprintf(err, err_size, "Rede indisponivel no console");
        return -1;
    }

    if (query == NULL || query[0] == '\0')
        return 0;

    if (proxy_url[0] != '\0')
        return search_proxy(query, tracks, max, found, err, err_size);

    return search_innertube(query, tracks, max, found, err, err_size);
}

/* ------------------------------------------------------------------ */
/* audio da faixa                                                     */
/* ------------------------------------------------------------------ */

static int looks_like_mp3(const unsigned char *data, size_t size)
{
    size_t i;

    if (size < 4)
        return 0;

    if (data[0] == 'I' && data[1] == 'D' && data[2] == '3')
        return 1;

    for (i = 0; i + 1 < size && i < 4096; i++)
    {
        if (data[i] == 0xff && (data[i + 1] & 0xe0) == 0xe0)
            return 1;
    }

    return 0;
}

static int build_direct_stream_url(const char *video_id, char *url,
                                   size_t url_size, char *err, size_t err_size)
{
    char body[1024];
    httpc_response res;
    json_object *root, *status, *streaming, *formats;
    int ok = -1;

    snprintf(body, sizeof(body),
             "{\"context\":{\"client\":{\"clientName\":\"ANDROID_VR\","
             "\"clientVersion\":\"1.65.10\",\"androidSdkVersion\":32,"
             "\"osName\":\"Android\",\"osVersion\":\"12\",\"hl\":\"pt\","
             "\"gl\":\"BR\"}},\"videoId\":\"%s\",\"contentCheckOk\":true,"
             "\"racyCheckOk\":true}",
             video_id);

    if (httpc_post_json(INNERTUBE_PLAYER, body, &res, 25) != 0 ||
        res.data == NULL)
    {
        snprintf(err, err_size, "Falha ao consultar o player");
        if (res.data)
            httpc_response_free(&res);
        return -1;
    }

    root = json_tokener_parse(res.data);
    httpc_response_free(&res);

    if (root == NULL)
    {
        snprintf(err, err_size, "JSON invalido do player");
        return -1;
    }

    if (json_object_object_get_ex(root, "playabilityStatus", &status))
    {
        json_object *state, *reason;

        if (json_object_object_get_ex(status, "status", &state))
        {
            const char *s = json_object_get_string(state);

            if (s == NULL || strcmp(s, "OK") != 0)
            {
                if (json_object_object_get_ex(status, "reason", &reason))
                    snprintf(err, err_size, "YouTube: %s",
                             json_object_get_string(reason));
                else
                    snprintf(err, err_size,
                             "Faixa bloqueada pelo YouTube (sem PO token)");
                json_object_put(root);
                return -1;
            }
        }
    }

    if (json_object_object_get_ex(root, "streamingData", &streaming) &&
        json_object_object_get_ex(streaming, "adaptiveFormats", &formats))
    {
        int i, n = json_object_array_length(formats);

        for (i = 0; i < n; i++)
        {
            json_object *f = json_object_array_get_idx(formats, i);
            json_object *mime, *u;

            if (f == NULL || !json_object_object_get_ex(f, "mimeType", &mime))
                continue;

            if (json_object_object_get_ex(f, "url", &u) &&
                strstr(json_object_get_string(mime), "audio/mp4") != NULL)
            {
                snprintf(url, url_size, "%s", json_object_get_string(u));
                ok = 0;
                break;
            }
        }
    }

    json_object_put(root);

    if (ok != 0)
        snprintf(err, err_size,
                 "Stream exige PO token; configure o proxy no arquivo "
                 "/dev_hdd0/tmp/ytm-proxy.txt");

    return ok;
}

int ytm_open_track(const ytm_track *track, char *err, size_t err_size)
{
    unsigned char *audio = NULL;
    size_t audio_len = 0;
    char url[700];
    int status;

    if (track == NULL)
        return -1;

    err[0] = '\0';

    if (track->stream_url[0] != '\0')
        snprintf(url, sizeof(url), "%s", track->stream_url);
    else if (proxy_url[0] != '\0')
        snprintf(url, sizeof(url), "%s/stream?id=%s", proxy_url, track->id);
    else if (build_direct_stream_url(track->id, url, sizeof(url), err,
                                     err_size) != 0)
        return -1;

    printf("YTM: baixando %s\n", url);
    status = httpc_get_binary(url, &audio, &audio_len, MAX_MP3_BYTES, 40);

    if (status < 0 || audio == NULL || audio_len == 0)
    {
        snprintf(err, err_size, "Nao foi possivel baixar o audio (HTTP %d)",
                 status);
        if (audio)
            free(audio);
        return -1;
    }

    if (!looks_like_mp3(audio, audio_len))
    {
        snprintf(err, err_size,
                 "Formato nao suportado: o proxy precisa entregar MP3");
        free(audio);
        return -1;
    }

    if (audio_load_mp3(audio, audio_len) != 0)
    {
        snprintf(err, err_size, "Falha ao iniciar o decoder de audio");
        free(audio);
        return -1;
    }

    return 0;
}
