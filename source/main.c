/* main.c - CELL MUSIC
 *
 * Primeira tela nativa do CELL MUSIC no PlayStation 3, desenhada pelo RSX:
 * fundo preto, detalhes vermelhos, barra de titulo, menu lateral
 * (Inicio / Biblioteca), lista central de musicas e barra de reproducao.
 *
 * Toda a interface usa apenas PSL1GHT (libgcm_sys / librsx / libio) e a
 * fonte bitmap 8x8 embutida em fonte8x8.h. Nao ha HTML, navegador, SDL,
 * audio ou rede nesta etapa.
 *
 * A navegacao ja responde ao controle (setas e X); a acao de reproducao
 * sera ligada ao audio numa etapa posterior.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <io/pad.h>

#include "gfx.h"

#define MAX_TRACKS 6

typedef struct
{
    const char *title;
    const char *artist;
    const char *duration;
} track_info;

typedef struct
{
    int focus;      /* 0 = menu lateral, 1 = lista central */
    int nav;        /* item do menu lateral */
    int track;      /* item da lista central */
    int playing;    /* faixa mostrada na barra de reproducao */
} app_state;

typedef struct
{
    int up;
    int down;
    int left;
    int right;
    int cross;
    int start;
} input_state;

static const char *nav_items[] = { "Inicio", "Biblioteca" };
static const int nav_count = (int)(sizeof(nav_items) / sizeof(nav_items[0]));

static const track_info recent_tracks[MAX_TRACKS] = {
    { "In the Air Tonight", "Phil Collins", "05:32" },
    { "Someone Like You", "Adele", "04:45" },
    { "Flowers", "Miley Cyrus", "03:20" },
    { "The Sound of Silence", "Disturbed", "04:08" },
    { "Bob Marley - Everything", "Mislav Vonic", "05:12" },
    { "Made For Me", "Muni Long", "03:41" },
};

static const track_info library_tracks[MAX_TRACKS] = {
    { "Killer In The Mirror", "Set It Off", "03:11" },
    { "Different Songs", "Set It Off", "03:46" },
    { "Midnight Thoughts", "Set It Off", "02:57" },
    { "Hourglass", "Set It Off", "02:57" },
    { "Go To Bed Angry", "Set It Off", "03:11" },
    { "Happy All The Time", "Set It Off", "03:39" },
};

static app_state app;
static input_state previous_input;

/* ------------------------------------------------------------------ */
/* utilidades de desenho                                              */
/* ------------------------------------------------------------------ */

static void draw_triangle_right(f32 x, f32 y, f32 w, f32 h, u32 color)
{
    /* aproximacao por colunas: suficiente para os icones desta tela */
    const int slices = 10;
    int i;

    for (i = 0; i < slices; i++)
    {
        f32 t = (f32)i / (f32)slices;
        f32 col_w = w / (f32)slices;
        f32 col_h = h * (1.0f - t);
        f32 col_y = y + (h - col_h) * 0.5f;

        gfxRect(x + (f32)i * col_w, col_y, col_w + 0.5f, col_h, color);
    }
}

static void draw_triangle_left(f32 x, f32 y, f32 w, f32 h, u32 color)
{
    const int slices = 10;
    int i;

    for (i = 0; i < slices; i++)
    {
        f32 t = (f32)i / (f32)slices;
        f32 col_w = w / (f32)slices;
        f32 col_h = h * (1.0f - t);
        f32 col_y = y + (h - col_h) * 0.5f;

        gfxRect(x + w - (f32)(i + 1) * col_w, col_y, col_w + 0.5f, col_h,
                color);
    }
}

static void draw_thumbnail(f32 x, f32 y, f32 size, int accent)
{
    gfxRect(x, y, size, size, GFX_ARGB(0xff, 0x24, 0x24, 0x24));

    if (accent)
    {
        f32 inner = size * 0.34f;

        gfxRect(x + (size - inner) * 0.5f, y + (size - inner) * 0.5f,
                inner, inner, GFX_RED);
    }
}

/* ------------------------------------------------------------------ */
/* telas                                                              */
/* ------------------------------------------------------------------ */

static void draw_top_bar(f32 s)
{
    f32 h = 64.0f * s;
    f32 logo = 22.0f * s;

    gfxRect(0, 0, (f32)gfxWidth(), h, GFX_ARGB(0xff, 0x0b, 0x0b, 0x0b));
    gfxRect(0, h - 2.0f * s, (f32)gfxWidth(), 2.0f * s, GFX_RED);

    /* marca: quadrado vermelho com M preto */
    gfxRect(24.0f * s, (h - logo) * 0.5f, logo, logo, GFX_RED);
    gfxText(24.0f * s + 3.0f * s, (h - logo) * 0.5f + 3.0f * s, 2.0f * s,
            GFX_BLACK, "M");

    gfxText(58.0f * s, (h - 24.0f * s) * 0.5f, 3.0f * s, GFX_WHITE,
            "CELL MUSIC");

    gfxText((f32)gfxWidth() - 120.0f * s, (h - 16.0f * s) * 0.5f, 2.0f * s,
            GFX_GRAY, "PS3");
}

static void draw_sidebar(f32 s)
{
    f32 top = 64.0f * s;
    f32 bottom = (f32)gfxHeight() - 96.0f * s;
    f32 width = 280.0f * s;
    f32 item_h = 52.0f * s;
    int i;

    gfxRect(0, top, width, bottom - top, GFX_ARGB(0xff, 0x0e, 0x0e, 0x0e));
    gfxRect(width - 2.0f * s, top, 2.0f * s, bottom - top,
            GFX_ARGB(0xff, 0x1c, 0x1c, 0x1c));

    for (i = 0; i < nav_count; i++)
    {
        f32 y = top + 28.0f * s + (f32)i * (item_h + 8.0f * s);
        int selected = (app.nav == i);
        int focused = selected && (app.focus == 0);

        if (selected)
        {
            gfxRect(16.0f * s, y, width - 32.0f * s, item_h,
                    GFX_ARGB(0xff, 0x1c, 0x1c, 0x1c));
            gfxRect(16.0f * s, y, 4.0f * s, item_h, GFX_RED);
        }

        /* icone */
        gfxRect(40.0f * s, y + (item_h - 14.0f * s) * 0.5f, 14.0f * s,
                14.0f * s, focused ? GFX_RED : GFX_GRAY_DARK);

        gfxText(68.0f * s, y + (item_h - 16.0f * s) * 0.5f, 2.0f * s,
                focused ? GFX_WHITE : GFX_GRAY, nav_items[i]);
    }

    gfxText(24.0f * s, bottom - 42.0f * s, 1.5f * s, GFX_GRAY_DARK,
            "SETAS + X");
}

static void draw_track_list(f32 s)
{
    const track_info *tracks;
    f32 x = 280.0f * s;
    f32 top = 64.0f * s;
    f32 width = (f32)gfxWidth() - x;
    f32 cursor = top + 40.0f * s;
    f32 row_h = 72.0f * s;
    const char *heading;
    const char *subheading;
    int i;

    if (app.nav == 0)
    {
        heading = "Tocadas recentemente";
        subheading = "Com base no seu historico da biblioteca";
        tracks = recent_tracks;
    }
    else
    {
        heading = "Biblioteca";
        subheading = "Todas as musicas do CELL MUSIC";
        tracks = library_tracks;
    }

    gfxText(x + 32.0f * s, cursor, 3.0f * s, GFX_WHITE, heading);
    cursor += 34.0f * s;
    gfxText(x + 32.0f * s, cursor, 1.5f * s, GFX_GRAY, subheading);
    cursor += 26.0f * s;

    /* cartao com a lista */
    gfxRect(x + 24.0f * s, cursor, width - 48.0f * s,
            MAX_TRACKS * row_h + 16.0f * s, GFX_ARGB(0xff, 0x11, 0x11, 0x11));

    for (i = 0; i < MAX_TRACKS; i++)
    {
        f32 y = cursor + 8.0f * s + (f32)i * row_h;
        int selected = (app.track == i);
        int playing = (app.playing == i);

        if (selected)
        {
            gfxRect(x + 24.0f * s, y, width - 48.0f * s, row_h,
                    GFX_ARGB(0xff, 0x1b, 0x1b, 0x1b));
            gfxRect(x + 24.0f * s, y, 4.0f * s, row_h,
                    (app.focus == 1) ? GFX_RED : GFX_RED_DARK);
        }

        draw_thumbnail(x + 44.0f * s, y + 12.0f * s, 48.0f * s, playing);

        gfxText(x + 108.0f * s, y + 18.0f * s, 2.0f * s,
                playing ? GFX_RED : GFX_WHITE, tracks[i].title);
        gfxText(x + 108.0f * s, y + 44.0f * s, 1.5f * s, GFX_GRAY,
                tracks[i].artist);
        gfxText(x + width - 72.0f * s - gfxTextWidth(tracks[i].duration, 1.5f * s),
                y + 44.0f * s, 1.5f * s, GFX_GRAY, tracks[i].duration);
    }
}

static void draw_player(f32 s)
{
    const track_info *tracks = (app.nav == 0) ? recent_tracks : library_tracks;
    const track_info *now = &tracks[app.playing % MAX_TRACKS];
    f32 h = 96.0f * s;
    f32 y = (f32)gfxHeight() - h;
    f32 bar_x = 560.0f * s;
    f32 bar_w = (f32)gfxWidth() - bar_x - 250.0f * s;
    f32 bar_y = y + 34.0f * s;
    f32 progress = 0.35f;
    f32 controls_x = (f32)gfxWidth() - 220.0f * s;

    gfxRect(0, y, (f32)gfxWidth(), h, GFX_ARGB(0xff, 0x0a, 0x0a, 0x0a));
    gfxRect(0, y, (f32)gfxWidth(), 2.0f * s, GFX_RED);

    draw_thumbnail(24.0f * s, y + 20.0f * s, 56.0f * s, 1);

    gfxText(96.0f * s, y + 26.0f * s, 2.0f * s, GFX_WHITE, now->title);
    gfxText(96.0f * s, y + 52.0f * s, 1.5f * s, GFX_GRAY, now->artist);

    /* barra de progresso */
    gfxRect(bar_x, bar_y, bar_w, 4.0f * s, GFX_ARGB(0xff, 0x2a, 0x2a, 0x2a));
    gfxRect(bar_x, bar_y, bar_w * progress, 4.0f * s, GFX_RED);
    gfxRect(bar_x + bar_w * progress - 3.0f * s, bar_y - 2.0f * s,
            6.0f * s, 8.0f * s, GFX_WHITE);

    gfxText(bar_x, bar_y + 14.0f * s, 1.5f * s, GFX_GRAY, "02:26");
    gfxText(bar_x + bar_w - gfxTextWidth("-01:09", 1.5f * s),
            bar_y + 14.0f * s, 1.5f * s, GFX_GRAY, "-01:09");

    /* controles */
    draw_triangle_left(controls_x, y + 38.0f * s, 14.0f * s, 20.0f * s,
                       GFX_GRAY);
    gfxRect(controls_x + 24.0f * s, y + 34.0f * s, 4.0f * s, 28.0f * s,
            GFX_GRAY);

    gfxRect(controls_x + 52.0f * s, y + 30.0f * s, 36.0f * s, 36.0f * s,
            GFX_RED);
    draw_triangle_right(controls_x + 64.0f * s, y + 38.0f * s, 14.0f * s,
                        20.0f * s, GFX_BLACK);

    draw_triangle_right(controls_x + 112.0f * s, y + 38.0f * s, 14.0f * s,
                        20.0f * s, GFX_GRAY);
    gfxRect(controls_x + 130.0f * s, y + 34.0f * s, 4.0f * s, 28.0f * s,
            GFX_GRAY);
}

/* ------------------------------------------------------------------ */
/* entrada                                                            */
/* ------------------------------------------------------------------ */

static void read_input(padData *pad, input_state *state)
{
    memset(state, 0, sizeof(input_state));

    if (pad == NULL)
        return;

    state->up = pad->BTN_UP ? 1 : 0;
    state->down = pad->BTN_DOWN ? 1 : 0;
    state->left = pad->BTN_LEFT ? 1 : 0;
    state->right = pad->BTN_RIGHT ? 1 : 0;
    state->cross = pad->BTN_CROSS ? 1 : 0;
    state->start = pad->BTN_START ? 1 : 0;
}

static int just_pressed(int now, int before)
{
    return now && !before;
}

static void handle_input(const input_state *in)
{
    if (just_pressed(in->up, previous_input.up))
    {
        if (app.focus == 0)
            app.nav = (app.nav + nav_count - 1) % nav_count;
        else
            app.track = (app.track + MAX_TRACKS - 1) % MAX_TRACKS;
    }

    if (just_pressed(in->down, previous_input.down))
    {
        if (app.focus == 0)
            app.nav = (app.nav + 1) % nav_count;
        else
            app.track = (app.track + 1) % MAX_TRACKS;
    }

    if (just_pressed(in->left, previous_input.left))
        app.focus = 0;

    if (just_pressed(in->right, previous_input.right))
        app.focus = 1;

    if (just_pressed(in->cross, previous_input.cross))
    {
        app.focus = 1;
        app.playing = app.track; /* sem audio nesta etapa: so a selecao */
    }

    previous_input = *in;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void)
{
    padInfo pad_info;
    padData pad_data;
    input_state input;
    f32 s;

    printf("CELL MUSIC: iniciando tela RSX\n");

    if (gfxInit() != 0)
    {
        printf("CELL MUSIC: nao foi possivel iniciar o video/RSX\n");
        return 1;
    }

    printf("CELL MUSIC: tela pronta (%ux%u)\n", gfxWidth(), gfxHeight());

    ioPadInit(7);

    app.focus = 1;
    app.nav = 0;
    app.track = 0;
    app.playing = 0;
    memset(&previous_input, 0, sizeof(previous_input));

    s = (f32)gfxHeight() / 720.0f; /* layout dimensionado para 720p */

    for (;;)
    {
        memset(&pad_data, 0, sizeof(padData));

        if (ioPadGetInfo(&pad_info) == 0 && pad_info.status[0])
            ioPadGetData(0, &pad_data);

        read_input(&pad_data, &input);
        handle_input(&input);

        gfxBegin();
        draw_top_bar(s);
        draw_sidebar(s);
        draw_track_list(s);
        draw_player(s);
        gfxEnd();
    }

    ioPadEnd();
    gfxShutdown();

    return 0;
}
