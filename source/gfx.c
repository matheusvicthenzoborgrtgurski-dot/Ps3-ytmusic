/* gfx.c - modulo de video do CELL MUSIC (PSL1GHT/RSX)
 *
 * Estrutura baseada nos exemplos oficiais do PSL1GHT:
 *  - samples/graphics/blitting/source/rsxutil.cpp: rsxInit, videoConfigure,
 *    gcmSetDisplayBuffer, setRenderTarget, flip com vsync e espera limitada
 *  - samples/graphics/blitting/source/main.cpp: rsxSetTransferImage
 *    (GCM_TRANSFER_LOCAL_TO_LOCAL) para levar a imagem ao framebuffer
 *
 * Correcao importante em relacao a versao anterior (tela preta no console):
 * gcmSurface.type precisa de GCM_SURFACE_TYPE_LINEAR (1). A versao anterior
 * usava GCM_TEXTURE_LINEAR (2), que na verdade e o valor de
 * GCM_SURFACE_TYPE_SWIZZLE: o RSX lia o framebuffer como swizzled e a tela
 * ficava preta.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>

#include <sysutil/video.h>
#include <rsx/rsx.h>
#include <rsx/gcm_sys.h>

#include "gfx.h"
#include "fonte8x8.h"

#define CB_SIZE            0x100000
#define HOST_SIZE          (32 * 1024 * 1024)
#define GCM_LABEL_INDEX    255
#define FLIP_WAIT_US       200
#define FLIP_WAIT_TRIES    2500

#define GLYPH_W            8
#define GLYPH_H            8

typedef struct
{
    u32 *ptr;
    u32 offset;
    u16 width;
    u16 height;
    int id;
} display_buffer;

static gcmContextData *ctx = NULL;
static void *host_mem = NULL;

static display_buffer buffers[2];
static int current_buffer = 0;
static int first_frame = 1;

static u16 screen_w = 0;
static u16 screen_h = 0;

static u32 *depth_mem = NULL;
static u32 depth_offset = 0;
static u32 depth_pitch = 0;

/* superficie da interface (ARGB) em memoria do RSX */
static u32 *ui_pixels = NULL;
static u32 ui_offset = 0;

static u32 frame_count = 0;
static int initialized = 0;

/* ------------------------------------------------------------------ */
/* espera e apresentacao                                              */
/* ------------------------------------------------------------------ */

static void wait_flip(void)
{
    int tries = 0;

    /* espera limitada: nunca deixa o aplicativo preso se o flip nao chegar */
    while (gcmGetFlipStatus() != 0 && tries < FLIP_WAIT_TRIES)
    {
        usleep(FLIP_WAIT_US);
        tries++;
    }

    gcmResetFlipStatus();
}

static int present(int buffer)
{
    if (gcmSetFlip(ctx, buffer) != 0)
        return -1;

    rsxFlushBuffer(ctx);
    gcmSetWaitFlip(ctx);

    return 0;
}

static int make_buffer(display_buffer *buffer, u16 width, u16 height, int id)
{
    u32 size = (u32)width * (u32)height * sizeof(u32);

    buffer->ptr = (u32 *)rsxMemalign(64, size);
    if (buffer->ptr == NULL)
        return -1;

    if (rsxAddressToOffset(buffer->ptr, &buffer->offset) != 0)
        return -1;

    if (gcmSetDisplayBuffer(id, buffer->offset, width * sizeof(u32),
                            width, height) != 0)
        return -1;

    buffer->width = width;
    buffer->height = height;
    buffer->id = id;

    memset(buffer->ptr, 0, size);

    return 0;
}

static void set_render_target(display_buffer *buffer)
{
    gcmSurface sf;

    memset(&sf, 0, sizeof(gcmSurface));

    sf.colorFormat = GCM_SURFACE_X8R8G8B8;
    sf.colorTarget = GCM_SURFACE_TARGET_0;
    sf.colorLocation[0] = GCM_LOCATION_RSX;
    sf.colorOffset[0] = buffer->offset;
    sf.colorPitch[0] = screen_w * sizeof(u32);

    sf.colorLocation[1] = GCM_LOCATION_RSX;
    sf.colorLocation[2] = GCM_LOCATION_RSX;
    sf.colorLocation[3] = GCM_LOCATION_RSX;
    sf.colorOffset[1] = 0;
    sf.colorOffset[2] = 0;
    sf.colorOffset[3] = 0;
    sf.colorPitch[1] = 64;
    sf.colorPitch[2] = 64;
    sf.colorPitch[3] = 64;

    sf.depthFormat = GCM_SURFACE_ZETA_Z16;
    sf.depthLocation = GCM_LOCATION_RSX;
    sf.depthOffset = depth_offset;
    sf.depthPitch = depth_pitch;

    /* ATENCAO: GCM_SURFACE_TYPE_LINEAR (1); GCM_TEXTURE_LINEAR vale 2 e
     * significa SWIZZLE, o que deixava a tela preta. */
    sf.type = GCM_SURFACE_TYPE_LINEAR;
    sf.antiAlias = GCM_SURFACE_CENTER_1;

    sf.width = buffer->width;
    sf.height = buffer->height;
    sf.x = 0;
    sf.y = 0;

    rsxSetSurface(ctx, &sf);
}

/* ------------------------------------------------------------------ */
/* init / shutdown                                                    */
/* ------------------------------------------------------------------ */

static void wait_rsx_idle(void)
{
    u32 label = 1;

    rsxSetWriteBackendLabel(ctx, GCM_LABEL_INDEX, label);
    rsxSetWaitLabel(ctx, GCM_LABEL_INDEX, label);
    rsxFlushBuffer(ctx);
    while (*(vu32 *)gcmGetLabelAddress(GCM_LABEL_INDEX) != label)
        usleep(30);
}

static gcmContextData *init_screen(void)
{
    gcmContextData *new_ctx = NULL;
    videoState state;
    videoConfiguration vconfig;
    videoResolution res;

    host_mem = memalign(1024 * 1024, HOST_SIZE);
    if (host_mem == NULL)
    {
        printf("CELL MUSIC: sem memoria de host para o RSX\n");
        return NULL;
    }

    if (rsxInit(&new_ctx, CB_SIZE, HOST_SIZE, host_mem) != 0 || new_ctx == NULL)
    {
        printf("CELL MUSIC: rsxInit falhou\n");
        return NULL;
    }

    ctx = new_ctx;

    if (videoGetState(0, 0, &state) != 0)
        return NULL;

    if (state.state != 0)
    {
        printf("CELL MUSIC: display desabilitado (state=%u)\n",
               (unsigned)state.state);
        return NULL;
    }

    if (videoGetResolution(state.displayMode.resolution, &res) != 0)
        return NULL;

    memset(&vconfig, 0, sizeof(videoConfiguration));
    vconfig.resolution = state.displayMode.resolution;
    vconfig.format = VIDEO_BUFFER_FORMAT_XRGB;
    vconfig.pitch = res.width * sizeof(u32);
    vconfig.aspect = state.displayMode.aspect;

    wait_rsx_idle();

    if (videoConfigure(0, &vconfig, NULL, 0) != 0)
    {
        printf("CELL MUSIC: videoConfigure falhou\n");
        return NULL;
    }

    /* releitura depois de configurar (como nos exemplos do PSL1GHT) */
    if (videoGetState(0, 0, &state) != 0)
        return NULL;

    if (videoGetResolution(state.displayMode.resolution, &res) != 0)
        return NULL;

    screen_w = res.width;
    screen_h = res.height;

    gcmSetFlipMode(GCM_FLIP_VSYNC);

    depth_pitch = screen_w * sizeof(u32);
    depth_mem = (u32 *)rsxMemalign(64, (u32)screen_h * depth_pitch * 2);
    if (depth_mem == NULL)
        return NULL;

    if (rsxAddressToOffset(depth_mem, &depth_offset) != 0)
        return NULL;

    gcmResetFlipStatus();

    return ctx;
}

int gfxInit(void)
{
    int i;

    if (initialized)
        return 0;

    if (init_screen() == NULL)
        return -1;

    for (i = 0; i < 2; i++)
    {
        if (make_buffer(&buffers[i], screen_w, screen_h, i) != 0)
        {
            printf("CELL MUSIC: falha ao criar o buffer de video %d\n", i);
            return -1;
        }
    }

    ui_pixels = (u32 *)rsxMemalign(64, (u32)screen_w * screen_h * sizeof(u32));
    if (ui_pixels == NULL)
    {
        printf("CELL MUSIC: falha ao alocar a superficie da interface\n");
        return -1;
    }

    if (rsxAddressToOffset(ui_pixels, &ui_offset) != 0)
        return -1;

    memset(ui_pixels, 0, (size_t)screen_w * screen_h * sizeof(u32));

    set_render_target(&buffers[current_buffer]);

    first_frame = 1;
    frame_count = 0;
    initialized = 1;

    printf("CELL MUSIC: video pronto em %ux%u\n", screen_w, screen_h);

    return 0;
}

void gfxShutdown(void)
{
    int i;

    if (!initialized)
        return;

    gcmSetWaitFlip(ctx);

    for (i = 0; i < 2; i++)
    {
        if (buffers[i].ptr)
            rsxFree(buffers[i].ptr);
    }

    if (depth_mem)
        rsxFree(depth_mem);

    if (ui_pixels)
        rsxFree(ui_pixels);

    rsxFinish(ctx, 1);

    if (host_mem)
        free(host_mem);

    ctx = NULL;
    depth_mem = NULL;
    ui_pixels = NULL;
    host_mem = NULL;
    initialized = 0;
}

u16 gfxWidth(void) { return screen_w; }
u16 gfxHeight(void) { return screen_h; }
u32 gfxFrameCount(void) { return frame_count; }

/* ------------------------------------------------------------------ */
/* rasterizacao                                                       */
/* ------------------------------------------------------------------ */

static inline u32 blend_pixel(u32 dst, u32 src)
{
    u32 a = (src >> 24) & 0xff;
    u32 inv;
    u32 r, g, b;

    if (a == 0xff)
        return src;

    if (a == 0)
        return dst;

    inv = 255 - a;

    r = (((src >> 16) & 0xff) * a + ((dst >> 16) & 0xff) * inv) / 255;
    g = (((src >> 8) & 0xff) * a + ((dst >> 8) & 0xff) * inv) / 255;
    b = ((src & 0xff) * a + (dst & 0xff) * inv) / 255;

    return 0xff000000u | (r << 16) | (g << 8) | b;
}

static void fill_rect(int x0, int y0, int x1, int y1, u32 color)
{
    int x, y;

    if (ui_pixels == NULL)
        return;

    if (x0 < 0)
        x0 = 0;
    if (y0 < 0)
        y0 = 0;
    if (x1 > (int)screen_w)
        x1 = screen_w;
    if (y1 > (int)screen_h)
        y1 = screen_h;

    if (x0 >= x1 || y0 >= y1)
        return;

    for (y = y0; y < y1; y++)
    {
        u32 *row = ui_pixels + (size_t)y * screen_w;

        if ((color >> 24) == 0xff)
        {
            for (x = x0; x < x1; x++)
                row[x] = color;
        }
        else
        {
            for (x = x0; x < x1; x++)
                row[x] = blend_pixel(row[x], color);
        }
    }
}

void gfxFrameBegin(u32 background)
{
    if (ui_pixels == NULL)
        return;

    fill_rect(0, 0, screen_w, screen_h, background);
}

void gfxRect(f32 x, f32 y, f32 w, f32 h, u32 color)
{
    fill_rect((int)(x + 0.5f), (int)(y + 0.5f),
              (int)(x + w + 0.5f), (int)(y + h + 0.5f), color);
}

void gfxRectOutline(f32 x, f32 y, f32 w, f32 h, f32 thickness, u32 color)
{
    if (thickness <= 0.0f)
        return;

    gfxRect(x, y, w, thickness, color);
    gfxRect(x, y + h - thickness, w, thickness, color);
    gfxRect(x, y, thickness, h, color);
    gfxRect(x + w - thickness, y, thickness, h, color);
}

void gfxText(f32 x, f32 y, f32 scale, u32 color, const char *text)
{
    f32 cursor = x;
    const char *p;

    if (ui_pixels == NULL || text == NULL)
        return;

    for (p = text; *p != '\0'; p++)
    {
        unsigned char ch = (unsigned char)*p;

        if (ch < FONT8X8_FIRST_CHAR || ch > FONT8X8_LAST_CHAR)
            continue;

        if (ch != ' ')
        {
            const unsigned char *glyph = font8x8_basic[ch - FONT8X8_FIRST_CHAR];
            int row, col;

            for (row = 0; row < GLYPH_H; row++)
            {
                unsigned char bits = glyph[row];

                if (bits == 0)
                    continue;

                for (col = 0; col < GLYPH_W; col++)
                {
                    int px0, py0, px1, py1;

                    if ((bits & (1 << col)) == 0)
                        continue;

                    px0 = (int)(cursor + (f32)col * scale + 0.5f);
                    px1 = (int)(cursor + (f32)(col + 1) * scale + 0.5f);
                    py0 = (int)(y + (f32)row * scale + 0.5f);
                    py1 = (int)(y + (f32)(row + 1) * scale + 0.5f);

                    if (px1 <= px0)
                        px1 = px0 + 1;
                    if (py1 <= py0)
                        py1 = py0 + 1;

                    fill_rect(px0, py0, px1, py1, color);
                }
            }
        }

        cursor += (GLYPH_W + 1) * scale;
    }
}

f32 gfxTextWidth(const char *text, f32 scale)
{
    f32 width = 0.0f;
    const char *p;

    if (text == NULL)
        return 0.0f;

    for (p = text; *p != '\0'; p++)
    {
        unsigned char ch = (unsigned char)*p;

        if (ch >= FONT8X8_FIRST_CHAR && ch <= FONT8X8_LAST_CHAR)
            width += (GLYPH_W + 1) * scale;
    }

    return width;
}

f32 gfxTextHeight(f32 scale)
{
    return (f32)GLYPH_H * scale;
}

/* ------------------------------------------------------------------ */
/* apresentacao                                                       */
/* ------------------------------------------------------------------ */

void gfxFrameEnd(void)
{
    size_t bytes;

    if (!initialized)
        return;

    if (!first_frame)
        wait_flip();
    else
        gcmResetFlipStatus();

    bytes = (size_t)screen_w * screen_h * sizeof(u32);

    /* Caminho 1: copia direta da PPU para o buffer que sera exibido. */
    memcpy(buffers[current_buffer].ptr, ui_pixels, bytes);

    /* Caminho 2: mesma imagem via motor de blit do RSX (exemplo oficial
     * blitting). Os dois caminhos levam o mesmo conteudo ao framebuffer,
     * de modo que a interface aparece se qualquer um dos dois funcionar. */
    rsxSetTransferImage(ctx, GCM_TRANSFER_LOCAL_TO_LOCAL,
                        buffers[current_buffer].offset, screen_w * sizeof(u32),
                        0, 0,
                        ui_offset, screen_w * sizeof(u32),
                        0, 0,
                        screen_w, screen_h, sizeof(u32));

    if (present(current_buffer) != 0)
        printf("CELL MUSIC: falha no flip\n");

    current_buffer = 1 - current_buffer;
    set_render_target(&buffers[current_buffer]);

    first_frame = 0;
    frame_count++;
}
