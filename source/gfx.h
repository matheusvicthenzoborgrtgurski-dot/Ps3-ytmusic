/* gfx.h - camada grafica 2D do CELL MUSIC sobre PSL1GHT/RSX
 *
 * Desenha retangulos e texto (fonte bitmap 8x8 embutida) numa superficie
 * ARGB em memoria do RSX e apresenta o quadro com o motor de blit do RSX
 * (rsxSetTransferImage + gcmSetFlip), a mesma tecnica do exemplo oficial
 * samples/graphics/blitting do PSL1GHT.
 *
 * Nao depende de SDL, HTML, navegador, Cg, fonte externa ou imagem externa.
 */
#ifndef CELL_MUSIC_GFX_H
#define CELL_MUSIC_GFX_H

#include <ppu-types.h>

/* Cor no formato 0xAARRGGBB. */
#define GFX_ARGB(a, r, g, b) \
    ((u32)(((u32)(a) & 0xff) << 24 | ((u32)(r) & 0xff) << 16 | \
           ((u32)(g) & 0xff) << 8 | ((u32)(b) & 0xff)))

#define GFX_WHITE      GFX_ARGB(0xff, 0xff, 0xff, 0xff)
#define GFX_RED        GFX_ARGB(0xff, 0xe5, 0x39, 0x35)
#define GFX_RED_DARK   GFX_ARGB(0xff, 0x8e, 0x1c, 0x1c)
#define GFX_BLACK      GFX_ARGB(0xff, 0x00, 0x00, 0x00)
#define GFX_GRAY       GFX_ARGB(0xff, 0x8a, 0x8a, 0x8a)
#define GFX_GRAY_DARK  GFX_ARGB(0xff, 0x3a, 0x3a, 0x3a)

/* Inicializa RSX, buffers de video e a superficie da interface.
 * Retorna 0 em caso de sucesso. */
int gfxInit(void);

/* Encerra RSX e libera a memoria grafica. */
void gfxShutdown(void);

/* Resolucao detectada no console. */
u16 gfxWidth(void);
u16 gfxHeight(void);

/* Inicia o quadro atual e pinta o fundo de preto. */
void gfxBegin(void);

/* Retangulo preenchido, em pixels. */
void gfxRect(f32 x, f32 y, f32 w, f32 h, u32 color);

/* Texto monoespaçado com a fonte embutida; scale 1.0 = 8x8 pixels. */
void gfxText(f32 x, f32 y, f32 scale, u32 color, const char *text);

/* Largura em pixels que gfxText ocupa. */
f32 gfxTextWidth(const char *text, f32 scale);

/* Blita a superficie na tela com o RSX e apresenta o quadro (flip). */
void gfxEnd(void);

#endif /* CELL_MUSIC_GFX_H */
