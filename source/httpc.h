/* httpc.h - cliente HTTP/HTTPS do YouTube Music para PS3
 *
 * Usa libcurl (portlibs do ps3dev) com mbedTLS. Requer o modulo de rede do
 * sistema carregado (SYSMODULE_NET) e netInitialize() antes de usar.
 */
#ifndef CELL_MUSIC_HTTPC_H
#define CELL_MUSIC_HTTPC_H

#include <ppu-types.h>

typedef struct
{
    char *data;   /* corpo da resposta, sempre terminado em '\0' */
    size_t len;   /* tamanho do corpo (sem o terminador) */
    long status;  /* codigo HTTP, 0 se nao houve resposta */
} httpc_response;

/* Inicializa/limpa o libcurl. Retorna 0 em sucesso. */
int httpc_init(void);
void httpc_shutdown(void);

/* GET. Retorna 0 se a requisicao completou (mesmo com HTTP != 200). */
int httpc_get(const char *url, httpc_response *out, int timeout_s);

/* POST JSON. Retorna 0 se completou. */
int httpc_post_json(const char *url, const char *body, httpc_response *out,
                    int timeout_s);

/* Baixa bytes binarios (audio) para memoria. Retorna 0 em sucesso. */
int httpc_get_binary(const char *url, unsigned char **out, size_t *out_len,
                     size_t max_bytes, int timeout_s);

void httpc_response_free(httpc_response *res);

#endif /* CELL_MUSIC_HTTPC_H */
