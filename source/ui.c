/* ui.c - interface do YouTube Music para PS3
 *
 * Layout inspirado no YouTube Music para desktop: barra superior com pesquisa,
 * menu lateral, area de conteudo com cartoes/resultados e barra de reproducao.
 * Todos os dados vem do modulo ytm (InnerTube ou proxy) e o audio do modulo
 * audio (libmad + cellAudio).
 */
#include <stdio.h>
#include <string.h>

#include "ui.h"
#include "ytm.h"
#include "audio.h"

#define ROWS_VISIBLE 6
#define QUERY_MAX    48

enum
{
    UI_IDLE = 0,
    UI_SEARCHING,
    UI_RESULTS,
    UI_LOADING,
    UI_ERROR
};

enum
{
    FOCUS_SEARCH = 0,
    FOCUS_SIDE,
    FOCUS_LIST
};

static struct
{
    int state;
    int focus;
    int nav;
    int selected;
    int scroll;
    int search_edit;
    char query[QUERY_MAX + 1];
    char message[192];
    char notice[160];
    int exit_prompt;
    int exit_choice;
    int playing;
    ytm_track tracks[YTM_MAX_TRACKS];
    int track_count;
    int audio_ok;
} app;

static ui_input last_input;

/* geometria */
static f32 s = 1.0f;
static f32 top_h, side_w, player_h;
static f32 search_x, search_y, search_w, search_h;
static f32 list_x, list_y, list_w, row_h;
static f32 side_x, side_y, side_w2, side_item_h;

static const char *nav_items[3] = { "Inicio", "Explorar", "Biblioteca" };

/* ------------------------------------------------------------------ */
/* utilitarios de desenho                                             */
/* ------------------------------------------------------------------ */

static void measure(void)
{
    f32 w = (f32)gfxWidth();
    f32 h = (f32)gfxHeight();

    s = h / 720.0f;
    if (s <= 0.0f)
        s = 1.0f;

    top_h = 64.0f * s;
    side_w = 280.0f * s;
    player_h = 88.0f * s;

    search_x = w * 0.32f;
    search_y = 16.0f * s;
    search_w = w * 0.36f;
    search_h = 32.0f * s;
    if (search_x + search_w > w - 220.0f * s)
        search_w = w - 220.0f * s - search_x;

    list_x = side_w + 32.0f * s;
    list_y = top_h + 150.0f * s;
    list_w = w - list_x - 32.0f * s;
    row_h = 64.0f * s;

    side_x = 20.0f * s;
    side_y = top_h + 24.0f * s;
    side_w2 = side_w - 40.0f * s;
    side_item_h = 46.0f * s;
}

static void play_icon(f32 x, f32 y, f32 size, u32 color)
{
    int i;
    const int slices = 12;

    for (i = 0; i < slices; i++)
    {
        f32 t = (f32)i / (f32)slices;
        f32 col_w = size / (f32)slices;
        f32 col_h = size * (1.0f - t);

        gfxRect(x + (f32)i * col_w, y + (size - col_h) * 0.5f, col_w + 0.5f,
                col_h, color);
    }
}

static void pause_icon(f32 x, f32 y, f32 size, u32 color)
{
    gfxRect(x, y, size * 0.32f, size, color);
    gfxRect(x + size * 0.68f - size * 0.32f + size * 0.32f, y, size * 0.32f,
            size, color);
}

static void prev_icon(f32 x, f32 y, f32 size, u32 color)
{
    gfxRect(x, y, size * 0.18f, size, color);
    play_icon(x + size * 0.22f, y, size * 0.78f, color);
}

static void next_icon(f32 x, f32 y, f32 size, u32 color)
{
    play_icon(x, y, size * 0.78f, color);
    gfxRect(x + size * 0.82f, y, size * 0.18f, size, color);
}

static void magnifier(f32 x, f32 y, f32 size, u32 color)
{
    gfxRectOutline(x, y, size * 0.72f, size * 0.72f, 2.0f * s, color);
    gfxRect(x + size * 0.66f, y + size * 0.66f, size * 0.34f, 2.0f * s, color);
}

static void home_icon(f32 x, f32 y, f32 size, u32 color)
{
    gfxRect(x, y + size * 0.42f, size, size * 0.58f, color);
    gfxRect(x + size * 0.14f, y + size * 0.24f, size * 0.72f, size * 0.2f,
            color);
    gfxRect(x + size * 0.3f, y + size * 0.08f, size * 0.4f, size * 0.18f, color);
}

static void compass_icon(f32 x, f32 y, f32 size, u32 color)
{
    gfxRectOutline(x, y, size, size, 2.0f * s, color);
    gfxRect(x + size * 0.45f, y + size * 0.2f, size * 0.16f, size * 0.6f,
            color);
}

static void library_icon(f32 x, f32 y, f32 size, u32 color)
{
    gfxRect(x, y, size * 0.22f, size, color);
    gfxRect(x + size * 0.32f, y, size * 0.22f, size, color);
    gfxRect(x + size * 0.66f, y, size * 0.3f, size, color);
}

static void draw_cover(f32 x, f32 y, f32 size, int filled)
{
    gfxRect(x, y, size, size, GFX_ARGB(0xff, 0x21, 0x21, 0x21));

    if (filled)
    {
        f32 inner = size * 0.36f;

        gfxRect(x + (size - inner) * 0.5f, y + (size - inner) * 0.5f, inner,
                inner, GFX_RED);
    }
    else
    {
        gfxRect(x + size * 0.28f, y + size * 0.28f, size * 0.44f, size * 0.44f,
                GFX_ARGB(0xff, 0x2e, 0x2e, 0x2e));
    }
}

static void draw_cursor(f32 x, f32 y)
{
    int i;

    for (i = 0; i < 14; i++)
    {
        f32 row_w = (f32)i * 0.85f + 2.0f;

        gfxRect(x, y + (f32)i, row_w, 1.0f, GFX_BLACK);
        if (i >= 2)
            gfxRect(x + 1.0f, y + (f32)i, row_w - 2.0f, 1.0f, GFX_WHITE);
    }

    gfxRect(x + 1.0f, y + 14.0f, 2.0f, 8.0f, GFX_BLACK);
    gfxRect(x + 4.0f, y + 14.0f, 2.0f, 6.0f, GFX_BLACK);
}

static void text_center(f32 cx, f32 y, f32 scale, u32 color, const char *text)
{
    gfxText(cx - gfxTextWidth(text, scale) * 0.5f, y, scale, color, text);
}

static void fmt_time(int seconds, char *out, size_t size)
{
    if (seconds < 0)
        seconds = 0;

    snprintf(out, size, "%d:%02d", seconds / 60, seconds % 60);
}

/* ------------------------------------------------------------------ */
/* desenho das secoes                                                 */
/* ------------------------------------------------------------------ */

static void draw_top_bar(void)
{
    f32 w = (f32)gfxWidth();
    f32 logo = 30.0f * s;

    gfxRect(0, 0, w, top_h, GFX_ARGB(0xff, 0x03, 0x03, 0x03));

    /* menu */
    {
        f32 bx = 24.0f * s;
        f32 by = (top_h - 16.0f * s) * 0.5f;
        int i;

        for (i = 0; i < 3; i++)
            gfxRect(bx, by + (f32)i * 6.0f * s, 18.0f * s, 2.0f * s, GFX_WHITE);
    }

    /* logo do YouTube Music */
    gfxRect(56.0f * s, (top_h - logo) * 0.5f, logo, logo, GFX_RED);
    play_icon(56.0f * s + logo * 0.3f, (top_h - logo) * 0.5f + logo * 0.26f,
              logo * 0.5f, GFX_WHITE);
    gfxText(96.0f * s, (top_h - 20.0f * s) * 0.5f, 2.5f * s, GFX_WHITE,
            "YouTube Music");

    /* caixa de pesquisa */
    gfxRect(search_x, search_y, search_w, search_h,
            GFX_ARGB(0xff, 0x14, 0x14, 0x14));
    gfxRectOutline(search_x, search_y, search_w, search_h, 1.0f * s,
                   (app.focus == FOCUS_SEARCH) ? GFX_RED
                                               : GFX_ARGB(0xff, 0x2a, 0x2a, 0x2a));
    magnifier(search_x + 10.0f * s, search_y + 10.0f * s, 14.0f * s, GFX_GRAY);

    if (app.query[0] != '\0')
        gfxText(search_x + 34.0f * s,
                search_y + (search_h - gfxTextHeight(2.0f * s)) * 0.5f, 2.0f * s,
                GFX_WHITE, app.query);
    else
        gfxText(search_x + 34.0f * s,
                search_y + (search_h - gfxTextHeight(2.0f * s)) * 0.5f, 2.0f * s,
                GFX_GRAY_DARK, "Pesquisar musicas, artistas, albuns");

    if (app.search_edit && ((gfxFrameCount() / 20) % 2) == 0)
    {
        f32 caret = search_x + 34.0f * s + gfxTextWidth(app.query, 2.0f * s);

        gfxRect(caret, search_y + 6.0f * s, 2.0f * s, search_h - 12.0f * s,
                GFX_RED);
    }

    /* avatar */
    gfxRect(w - 64.0f * s, (top_h - 26.0f * s) * 0.5f, 26.0f * s, 26.0f * s,
            GFX_ARGB(0xff, 0x53, 0x53, 0x53));
    gfxText(w - 56.0f * s, (top_h - 14.0f * s) * 0.5f, 2.0f * s, GFX_WHITE, "M");
}

static void draw_sidebar(void)
{
    f32 h = (f32)gfxHeight();
    f32 bottom = h - player_h;
    int i;

    gfxRect(0, top_h, side_w, bottom - top_h, GFX_ARGB(0xff, 0x03, 0x03, 0x03));

    for (i = 0; i < 3; i++)
    {
        f32 y = side_y + (f32)i * (side_item_h + 4.0f * s);
        int selected = (app.nav == i);
        u32 color = selected ? GFX_WHITE : GFX_GRAY;
        f32 ix = side_x + 12.0f * s;
        f32 iy = y + (side_item_h - 16.0f * s) * 0.5f;

        if (selected)
        {
            gfxRect(side_x, y, side_w2, side_item_h,
                    GFX_ARGB(0xff, 0x14, 0x14, 0x14));
            gfxRect(side_x, y, 3.0f * s, side_item_h, GFX_RED);
        }

        if (i == 0)
            home_icon(ix, iy, 16.0f * s, color);
        else if (i == 1)
            compass_icon(ix, iy, 16.0f * s, color);
        else
            library_icon(ix, iy, 16.0f * s, color);

        gfxText(side_x + 44.0f * s,
                y + (side_item_h - gfxTextHeight(2.0f * s)) * 0.5f, 2.0f * s,
                color, nav_items[i]);
    }

    /* playlists */
    gfxText(side_x + 12.0f * s, side_y + 3.0f * (side_item_h + 4.0f * s) +
                                     20.0f * s,
            1.5f * s, GFX_GRAY_DARK, "PLAYLISTS");
    gfxText(side_x + 12.0f * s, side_y + 3.0f * (side_item_h + 4.0f * s) +
                                     44.0f * s,
            1.8f * s, GFX_GRAY, "Meu mix");

    /* estado dos dispositivos e da rede */
    {
        f32 y = bottom - 96.0f * s;

        gfxText(side_x + 8.0f * s, y, 1.5f * s,
                ytm_net_ok() ? GFX_GRAY_DARK : GFX_RED,
                ytm_net_ok() ? "REDE OK" : "REDE OFF");
        gfxText(side_x + 8.0f * s, y + 18.0f * s, 1.5f * s,
                app.audio_ok ? GFX_GRAY_DARK : GFX_RED,
                app.audio_ok ? "AUDIO OK" : "AUDIO OFF");
        gfxText(side_x + 8.0f * s, y + 36.0f * s, 1.5f * s,
                last_input.pad.connected ? GFX_GRAY_DARK : GFX_RED,
                last_input.pad.connected ? "CONTROLE OK" : "CONTROLE --");
        gfxText(side_x + 8.0f * s, y + 54.0f * s, 1.5f * s,
                last_input.kb.connected ? GFX_GRAY_DARK : GFX_RED,
                last_input.kb.connected ? "TECLADO OK" : "TECLADO --");
        gfxText(side_x + 8.0f * s, y + 72.0f * s, 1.5f * s,
                last_input.mouse.connected ? GFX_GRAY_DARK : GFX_RED,
                last_input.mouse.connected ? "MOUSE OK" : "MOUSE --");
    }
}

static void draw_idle(void)
{
    static const char *cards[6] = { "Mix diario 1", "Mix diario 2",
                                    "Descobertas", "Favoritas",
                                    "Tocadas recentes", "Novidades" };
    int i;

    gfxText(list_x, top_h + 48.0f * s, 3.5f * s, GFX_WHITE, "Boa noite");
    gfxText(list_x, top_h + 88.0f * s, 1.6f * s, GFX_GRAY_DARK,
            "Pesquise uma musica para comecar (TAB e digite)");

    for (i = 0; i < 6; i++)
    {
        int col = i % 3;
        int row = i / 3;
        f32 cw = (list_w - 40.0f * s) / 3.0f;
        f32 x = list_x + (f32)col * (cw + 20.0f * s);
        f32 y = list_y + (f32)row * (cw + 46.0f * s);

        draw_cover(x, y, cw, 0);
        gfxText(x, y + cw + 10.0f * s, 1.8f * s, GFX_WHITE, cards[i]);
    }
}

static void draw_results(void)
{
    char heading[128];
    int i;

    snprintf(heading, sizeof(heading), "Resultados para \"%s\"", app.query);
    gfxText(list_x, top_h + 48.0f * s, 2.4f * s, GFX_WHITE, heading);

    if (app.notice[0] != '\0')
        gfxText(list_x, top_h + 82.0f * s, 1.5f * s, GFX_RED, app.notice);
    else
        gfxText(list_x, top_h + 82.0f * s, 1.5f * s, GFX_GRAY_DARK,
                "X/ENTER toca   SELECT abre o menu de saida");

    for (i = 0; i < ROWS_VISIBLE; i++)
    {
        int index = app.scroll + i;
        f32 y = list_y + (f32)i * row_h;
        ytm_track *t;
        char dur[16];
        int selected;

        if (index >= app.track_count)
            break;

        t = &app.tracks[index];
        selected = (index == app.selected);

        if (selected)
        {
            gfxRect(list_x - 12.0f * s, y, list_w + 12.0f * s, row_h,
                    GFX_ARGB(0xff, 0x1a, 0x1a, 0x1a));
            gfxRect(list_x - 12.0f * s, y, 3.0f * s, row_h,
                    (app.focus == FOCUS_LIST) ? GFX_RED : GFX_RED_DARK);
        }

        draw_cover(list_x + 8.0f * s, y + 8.0f * s, 48.0f * s,
                   app.playing == index);

        gfxText(list_x + 72.0f * s, y + 12.0f * s, 2.0f * s,
                (app.playing == index) ? GFX_RED : GFX_WHITE, t->title);

        {
            const char *kind = NULL;
            char subtitle[160];

            if (t->kind == YTM_KIND_ALBUM)
                kind = "Album";
            else if (t->kind == YTM_KIND_PLAYLIST)
                kind = "Playlist";
            else if (t->kind == YTM_KIND_ARTIST)
                kind = "Artista";

            if (kind != NULL && t->artist[0] != '\0')
                snprintf(subtitle, sizeof(subtitle), "%s - %s", kind, t->artist);
            else if (kind != NULL)
                snprintf(subtitle, sizeof(subtitle), "%s", kind);
            else
                snprintf(subtitle, sizeof(subtitle), "%s", t->artist);

            if (subtitle[0] != '\0')
                gfxText(list_x + 72.0f * s, y + 36.0f * s, 1.5f * s, GFX_GRAY,
                        subtitle);
        }

        if (t->duration > 0)
        {
            fmt_time(t->duration, dur, sizeof(dur));
            gfxText(list_x + list_w - 20.0f * s -
                        gfxTextWidth(dur, 1.5f * s),
                    y + 36.0f * s, 1.5f * s, GFX_GRAY, dur);
        }
    }

    if (app.track_count > ROWS_VISIBLE)
    {
        f32 track_h = (f32)ROWS_VISIBLE * row_h;
        f32 bar_h = track_h * (f32)ROWS_VISIBLE / (f32)app.track_count;
        f32 bar_y = list_y + track_h * (f32)app.scroll / (f32)app.track_count;

        gfxRect(list_x + list_w - 4.0f * s, list_y, 4.0f * s, track_h,
                GFX_ARGB(0xff, 0x22, 0x22, 0x22));
        gfxRect(list_x + list_w - 4.0f * s, bar_y, 4.0f * s, bar_h, GFX_RED);
    }
}

static void draw_content(void)
{
    switch (app.state)
    {
    case UI_SEARCHING:
        text_center((f32)gfxWidth() * 0.5f, (f32)gfxHeight() * 0.45f,
                    2.4f * s, GFX_WHITE, "Pesquisando no YouTube Music...");
        break;

    case UI_LOADING:
        text_center((f32)gfxWidth() * 0.5f, (f32)gfxHeight() * 0.45f,
                    2.4f * s, GFX_WHITE, "Carregando faixa...");
        break;

    case UI_ERROR:
        gfxText(list_x, top_h + 60.0f * s, 2.2f * s, GFX_RED, "Falha");
        gfxText(list_x, top_h + 100.0f * s, 1.8f * s, GFX_GRAY, app.message);
        gfxText(list_x, top_h + 140.0f * s, 1.5f * s, GFX_GRAY_DARK,
                "Verifique a rede em Ajustes da rede, ou configure o proxy em "
                "/dev_hdd0/tmp/ytm-proxy.txt");
        break;

    case UI_RESULTS:
        draw_results();
        break;

    case UI_IDLE:
    default:
        draw_idle();
        break;
    }
}

static void draw_player(void)
{
    f32 h = (f32)gfxHeight();
    f32 y = h - player_h;
    f32 w = (f32)gfxWidth();
    char cur[16] = "0:00", total[16] = "0:00";
    const ytm_track *t = NULL;
    f32 bar_x = 460.0f * s;
    f32 bar_w = w - bar_x - 200.0f * s;
    f32 bar_y = y + 40.0f * s;
    f32 progress = 0.0f;
    int pos = audio_position();
    int dur = audio_duration();

    if (app.playing >= 0 && app.playing < app.track_count)
        t = &app.tracks[app.playing];

    /* sem faixa carregada a barra fica zerada, mesmo que o decoder informe
     * numeros da faixa anterior */
    if (t == NULL)
    {
        pos = 0;
        dur = 0;
    }

    gfxRect(0, y, w, player_h, GFX_ARGB(0xff, 0x0a, 0x0a, 0x0a));

    draw_cover(20.0f * s, y + 16.0f * s, 56.0f * s, t != NULL);

    if (t != NULL)
    {
        gfxText(88.0f * s, y + 22.0f * s, 2.0f * s, GFX_WHITE, t->title);
        gfxText(88.0f * s, y + 46.0f * s, 1.5f * s, GFX_GRAY, t->artist);
    }
    else
    {
        gfxText(88.0f * s, y + 34.0f * s, 1.8f * s, GFX_GRAY_DARK,
                "Nenhuma faixa tocando");
    }

    if (dur > 0)
    {
        progress = (f32)pos / (f32)dur;
        if (progress > 1.0f)
            progress = 1.0f;
    }

    gfxRect(bar_x, bar_y, bar_w, 4.0f * s, GFX_ARGB(0xff, 0x2a, 0x2a, 0x2a));
    gfxRect(bar_x, bar_y, bar_w * progress, 4.0f * s, GFX_RED);
    gfxRect(bar_x + bar_w * progress - 3.0f * s, bar_y - 3.0f * s, 6.0f * s,
            10.0f * s, GFX_WHITE);

    fmt_time(pos, cur, sizeof(cur));
    fmt_time(dur, total, sizeof(total));
    gfxText(bar_x, bar_y + 14.0f * s, 1.5f * s, GFX_GRAY, cur);
    gfxText(bar_x + bar_w - gfxTextWidth(total, 1.5f * s), bar_y + 14.0f * s,
            1.5f * s, GFX_GRAY, total);

    /* controles */
    {
        f32 cx = w - 170.0f * s;

        prev_icon(cx, y + 30.0f * s, 20.0f * s, GFX_GRAY);
        if (audio_is_playing())
            pause_icon(cx + 44.0f * s, y + 28.0f * s, 24.0f * s, GFX_WHITE);
        else
            play_icon(cx + 44.0f * s, y + 28.0f * s, 24.0f * s, GFX_WHITE);
        next_icon(cx + 92.0f * s, y + 30.0f * s, 20.0f * s, GFX_GRAY);
    }
}

static void draw_exit_prompt(void)
{
    f32 w = 460.0f * s;
    f32 h = 190.0f * s;
    f32 x = ((f32)gfxWidth() - w) * 0.5f;
    f32 y = ((f32)gfxHeight() - h) * 0.5f;
    const char *labels[2] = { "Cancelar", "Sair" };
    int i;

    gfxRect(0, 0, (f32)gfxWidth(), (f32)gfxHeight(), GFX_ARGB(0xc0, 0, 0, 0));
    gfxRect(x, y, w, h, GFX_ARGB(0xff, 0x16, 0x16, 0x16));
    gfxRectOutline(x, y, w, h, 2.0f * s, GFX_RED);

    gfxText(x + 24.0f * s, y + 24.0f * s, 3.0f * s, GFX_WHITE,
            "Sair do YouTube Music?");
    gfxText(x + 24.0f * s, y + 68.0f * s, 1.5f * s, GFX_GRAY,
            "A reproducao sera interrompida.");

    for (i = 0; i < 2; i++)
    {
        f32 bx = x + 24.0f * s + (f32)i * 200.0f * s;
        f32 by = y + 108.0f * s;
        int active = (app.exit_choice == i);

        gfxRect(bx, by, 180.0f * s, 44.0f * s,
                active ? GFX_RED : GFX_ARGB(0xff, 0x24, 0x24, 0x24));
        gfxText(bx + 16.0f * s, by + 14.0f * s, 2.0f * s,
                active ? GFX_BLACK : GFX_WHITE, labels[i]);
    }

    gfxText(x + 24.0f * s, y + 160.0f * s, 1.5f * s, GFX_GRAY_DARK,
            "X confirma   O cancela");
}

void ui_draw(void)
{
    measure();

    gfxFrameBegin(GFX_BLACK);

    draw_top_bar();
    draw_sidebar();
    draw_content();
    draw_player();

    if (app.exit_prompt)
        draw_exit_prompt();

    if (last_input.mouse.connected)
        draw_cursor((f32)last_input.mouse.x, (f32)last_input.mouse.y);
}

/* ------------------------------------------------------------------ */
/* acoes                                                              */
/* ------------------------------------------------------------------ */

/* desenha um quadro imediatamente (antes de uma chamada de rede bloqueante) */
static void flush_frame(void)
{
    ui_draw();
    gfxFrameEnd();
}

static void do_search(void)
{
    char err[192];
    int found = 0;

    if (app.query[0] == '\0')
        return;

    app.state = UI_SEARCHING;
    app.message[0] = '\0';
    app.notice[0] = '\0';
    flush_frame();

    if (ytm_search(app.query, app.tracks, YTM_MAX_TRACKS, &found, err,
                   sizeof(err)) != 0)
    {
        snprintf(app.message, sizeof(app.message), "%s", err);
        app.state = UI_ERROR;
        app.track_count = 0;
        return;
    }

    app.track_count = found;
    app.selected = 0;
    app.scroll = 0;

    if (found == 0)
    {
        snprintf(app.message, sizeof(app.message), "%s", err);
        app.state = UI_ERROR;
        return;
    }

    app.state = UI_RESULTS;
    app.focus = FOCUS_LIST;
}

static void play_selected(void)
{
    char err[192];
    ytm_track *t;

    if (app.track_count <= 0 || app.selected >= app.track_count)
        return;

    t = &app.tracks[app.selected];

    if (t->kind != YTM_KIND_SONG)
    {
        snprintf(app.notice, sizeof(app.notice),
                 "%s: selecione uma musica (resultados deste tipo ainda nao "
                 "abrem)",
                 t->kind == YTM_KIND_ALBUM
                     ? "Album"
                     : (t->kind == YTM_KIND_PLAYLIST ? "Playlist" : "Artista"));
        return;
    }

    app.notice[0] = '\0';
    app.state = UI_LOADING;
    snprintf(app.message, sizeof(app.message), "%s", t->title);
    flush_frame();

    if (ytm_open_track(t, err, sizeof(err)) != 0)
    {
        snprintf(app.message, sizeof(app.message), "%s", err);
        app.state = UI_ERROR;
        return;
    }

    app.playing = app.selected;
    app.state = UI_RESULTS;
}

static void move_selection(int delta)
{
    if (app.state != UI_RESULTS || app.track_count == 0)
        return;

    app.selected += delta;

    if (app.selected < 0)
        app.selected = app.track_count - 1;
    if (app.selected >= app.track_count)
        app.selected = 0;

    if (app.selected < app.scroll)
        app.scroll = app.selected;
    if (app.selected >= app.scroll + ROWS_VISIBLE)
        app.scroll = app.selected - ROWS_VISIBLE + 1;
}

static void handle_mouse(void)
{
    f32 mx = (f32)last_input.mouse.x;
    f32 my = (f32)last_input.mouse.y;
    int i;

    if (app.exit_prompt)
    {
        f32 w = 460.0f * s;
        f32 h = 190.0f * s;
        f32 x = ((f32)gfxWidth() - w) * 0.5f;
        f32 y = ((f32)gfxHeight() - h) * 0.5f;
        f32 bx = x + 24.0f * s;
        f32 by = y + 108.0f * s;

        if (mx >= bx && mx <= bx + 180.0f * s && my >= by &&
            my <= by + 44.0f * s)
        {
            app.exit_prompt = 0;
            return;
        }

        if (mx >= bx + 200.0f * s && mx <= bx + 380.0f * s && my >= by &&
            my <= by + 44.0f * s)
        {
            app.exit_prompt = 2;
            return;
        }

        return;
    }

    if (mx >= search_x && mx <= search_x + search_w && my >= search_y &&
        my <= search_y + search_h)
    {
        app.focus = FOCUS_SEARCH;
        app.search_edit = 1;
        return;
    }

    for (i = 0; i < 3; i++)
    {
        f32 y = side_y + (f32)i * (side_item_h + 4.0f * s);

        if (mx >= side_x && mx <= side_x + side_w2 && my >= y &&
            my <= y + side_item_h)
        {
            app.nav = i;
            app.focus = FOCUS_SIDE;
            return;
        }
    }

    if (app.state == UI_RESULTS && mx >= list_x - 12.0f * s &&
        mx <= list_x - 12.0f * s + list_w)
    {
        f32 row = (my - list_y) / row_h;
        int index;

        if (row < 0.0f)
            return;

        index = app.scroll + (int)row;

        if (index >= 0 && index < app.track_count)
        {
            app.focus = FOCUS_LIST;
            app.selected = index;
            play_selected();
        }
    }
}

int ui_init(void)
{
    memset(&app, 0, sizeof(app));

    app.state = UI_IDLE;
    app.focus = FOCUS_SEARCH;
    app.nav = 0;
    app.selected = 0;
    app.playing = -1;
    app.audio_ok = audio_ready();

    return 0;
}

void ui_update(const ui_input *in, int *exit_requested)
{
    if (in == NULL || exit_requested == NULL)
        return;

    last_input = *in;

    if (in->kb.alt_f4)
    {
        *exit_requested = 1;
        return;
    }

    if (app.exit_prompt)
    {
        if (app.exit_prompt == 2)
        {
            *exit_requested = 1;
            return;
        }

        if (in->kb.esc || in->pad.circle)
        {
            app.exit_prompt = 0;
            return;
        }

        if (in->pad.left || in->pad.right || in->kb.left || in->kb.right)
            app.exit_choice = app.exit_choice ? 0 : 1;

        if (in->kb.enter || in->pad.cross)
        {
            if (app.exit_choice == 1)
                *exit_requested = 1;
            else
                app.exit_prompt = 0;
            return;
        }

        return;
    }

    if (in->mouse.left)
    {
        handle_mouse();

        if (app.exit_prompt == 2)
        {
            *exit_requested = 1;
            return;
        }
    }

    if (in->mouse.wheel != 0 && app.state == UI_RESULTS &&
        app.track_count > ROWS_VISIBLE)
    {
        int max_scroll = app.track_count - ROWS_VISIBLE;

        app.scroll += (in->mouse.wheel > 0) ? 1 : -1;

        if (app.scroll < 0)
            app.scroll = 0;
        if (app.scroll > max_scroll)
            app.scroll = max_scroll;
    }

    if (in->pad.select || in->pad.start)
    {
        app.exit_prompt = 1;
        app.exit_choice = 0;
        return;
    }

    /* teclado */
    if (in->kb.esc)
    {
        if (app.search_edit)
            app.search_edit = 0;
        else
        {
            app.exit_prompt = 1;
            app.exit_choice = 0;
        }
        return;
    }

    if (in->kb.tab)
    {
        app.search_edit = app.search_edit ? 0 : 1;
        app.focus = FOCUS_SEARCH;
    }

    if (app.search_edit && in->kb.text_len > 0)
    {
        int i;

        for (i = 0; i < in->kb.text_len; i++)
        {
            size_t len = strlen(app.query);

            if (len < QUERY_MAX)
            {
                app.query[len] = in->kb.text[i];
                app.query[len + 1] = '\0';
            }
        }
    }

    if (app.search_edit && in->kb.backspace)
    {
        size_t len = strlen(app.query);

        if (len > 0)
            app.query[len - 1] = '\0';
    }

    if (in->kb.enter)
    {
        if (app.search_edit || app.state != UI_RESULTS)
        {
            app.search_edit = 0;
            do_search();
            return;
        }

        play_selected();
        return;
    }

    /* navegacao */
    if (in->pad.up || in->kb.up)
    {
        if (app.state == UI_RESULTS)
            move_selection(-1);
    }

    if (in->pad.down || in->kb.down)
    {
        if (app.state == UI_RESULTS)
            move_selection(1);
    }

    if (in->pad.left || in->kb.left)
        app.focus = (app.focus == FOCUS_SEARCH) ? FOCUS_SEARCH : FOCUS_SIDE;

    if (in->pad.right || in->kb.right)
        app.focus = (app.focus == FOCUS_SIDE) ? FOCUS_LIST : app.focus;

    if (in->pad.cross)
    {
        if (app.state == UI_RESULTS)
            play_selected();
        else if (app.state != UI_SEARCHING && app.state != UI_LOADING)
            do_search();
    }
}
