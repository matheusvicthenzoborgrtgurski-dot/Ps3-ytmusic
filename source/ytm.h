/* ytm.h - cliente do YouTube Music para PS3
 *
 * Dois modos de operacao:
 *  1. Direto: HTTPS para music.youtube.com/youtubei/v1 (InnerTube, cliente
 *     WEB_REMIX) - pesquisa funciona sem login.
 *  2. Proxy: se houver um proxy configurado na rede local (Android/Termux com
 *     yt-dlp + PO token), a pesquisa e o audio MP3 vem dele, o que permite
 *     tocar as faixas que o YouTube bloqueia sem prova de origem.
 */
#ifndef CELL_MUSIC_YTM_H
#define CELL_MUSIC_YTM_H

#include <ppu-types.h>

#define YTM_MAX_TRACKS 24

/* Tipo do resultado, conforme o InnerTube informa na segunda coluna. */
#define YTM_KIND_SONG     0
#define YTM_KIND_ALBUM    1
#define YTM_KIND_PLAYLIST 2
#define YTM_KIND_ARTIST   3

typedef struct
{
    char id[32];
    char title[96];
    char artist[96];
    char album[64];
    int kind;
    int duration; /* segundos, 0 se desconhecida */
    char art_url[192];
    char stream_url[320]; /* preenchido pelo proxy, quando houver */
} ytm_track;

/* Carrega a rede (SYSMODULE_NET + netInitialize) e o libcurl. */
int ytm_init(void);
void ytm_shutdown(void);

/* 0 quando a rede esta utilizavel. */
int ytm_net_ok(void);

/* Origem configurada: "" (direto) ou "http://ip:porta". */
const char *ytm_proxy_url(void);

/* Pesquisa musicas/artistas/albuns/playlists. Retorna 0 e *found >= 0. */
int ytm_search(const char *query, ytm_track *tracks, int max, int *found,
               char *err, size_t err_size);

/* Obtem o MP3 da faixa e entrega ao modulo de audio. */
int ytm_open_track(const ytm_track *track, char *err, size_t err_size);

#endif /* CELL_MUSIC_YTM_H */
