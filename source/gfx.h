/* gfx.h - modulo de video do CELL MUSIC (PSL1GHT/RSX)
 *
 * Superficie ARGB em memoria do RSX, desenhada pela PPU e apresentada no
 * framebuffer com o motor de blit do RSX + flip com vsync. Tecnica baseada
 * nos exemplos oficiais samples/graphics/blitting e samples/graphics/videoTest.
 *
 * Sem SDL, sem HTML, sem navegador, sem fonte ou imagem externa.
 */
#ifndef CELL_MUSIC_GFX_H
#define CELL_MUSIC_GFX_H

#include <ppu-types.h>

/* Cor 0xAARRGGBB */
#define GFX_ARGB(a, r, g, b) \
    ((u32)(((u32)(a) & 0xff) << 24 | ((u32)(r) & 0xff) << 16 | \
           ((u32)(g) & 0xff) << 8 | ((u32)(b) & 0xff)))

#define GFX_BLACK      GFX_ARGB(0xff, 0x00, 0x00, 0x00)
#define GFX_WHITE      GFX_ARGB(0xff, 0xff, 0xff, 0xff)
#define GFX_RED        GFX_ARGB(0xff, 0xe5, 0x39, 0x35)
#define GFX_RED_DARK   GFX_ARGB(0xff, 0x8e, 0x1c, 0x1c)
#define GFX_GRAY       GFX_ARGB(0xff, 0x8a, 0x8a, 0x8a)
#define GFX_GRAY_DARK  GFX_ARGB(0xff, 0x3a, 0x3a, 0x3a)

/* Inicializa RSX e video. Retorna 0 em caso de sucesso. */
int gfxInit(void);

/* Encerra RSX e libera a memoria grafica. Seguro para chamar duas vezes. */
void gfxShutdown(void);

/* Estado do video. */
u16 gfxWidth(void);
u16 gfxHeight(void);

/* Inicia um quadro e limpa a superficie com a cor informada. */
void gfxFrameBegin(u32 background);

/* Primitivas. */
void gfxRect(f32 x, f32 y, f32 w, f32 h, u32 color);
void gfxRectOutline(f32 x, f32 y, f32 w, f32 h, f32 thickness, u32 color);
void gfxText(f32 x, f32 y, f32 scale, u32 color, const char *text);
f32 gfxTextWidth(const char *text, f32 scale);
f32 gfxTextHeight(f32 scale);

/* Apresenta o quadro: copia para o framebuffer, faz o flip e troca o buffer. */
void gfxFrameEnd(void);

/* Numero do quadro apresentado (util para animacoes simples). */
u32 gfxFrameCount(void);

#endif /* CELL_MUSIC_GFX_H */
