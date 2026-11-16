/* ui.c - interface do CELL MUSIC (desenhada com gfx.c, sem SDL)
 *
 * Contem o layout (barra superior, menu lateral, lista central, barra de
 * reproducao), a navegacao por controle/teclado/mouse, a pesquisa digitada e
 * o dialogo de encerramento.
 */
#include <stdio.h>
#include <string.h>

#include "ui.h"

#define MAX_TRACKS 8
#define ROWS_VISIBLE 6
#define SEARCH_MAX 28

typedef struct
{
    const char *title;
    const char *artist;
    const char *duration;
} track_info;

static const char *nav_items[] = { "Inicio", "Biblioteca" };
#define NAV_COUNT ((int)(sizeof(nav_items) / sizeof(nav_items[0])))

static const track_info recent_tracks[MAX_TRACKS] = {
    { "In the Air Tonight", "Phil Collins", "05:32" },
    { "Someone Like You", "Adele", "04:45" },
    { "Flowers", "Miley Cyrus", "03:20" },
    { "The Sound of Silence", "Disturbed", "04:08" },
    { "Bob Marley - Everything", "Mislav Vonic", "05:12" },
    { "Made For Me", "Muni Long", "03:41" },
    { "Enemy", "Imagine Dragons", "03:53" },
    { "As It Was", "Harry Styles", "02:47" },
};

static const track_info library_tracks[MAX_TRACKS] = {
    { "Killer In The Mirror", "Set It Off", "03:11" },
    { "Different Songs", "Set It Off", "03:46" },
    { "Midnight Thoughts", "Set It Off", "02:57" },
    { "Hourglass", "Set It Off", "02:57" },
    { "Go To Bed Angry", "Set It Off", "03:11" },
    { "Happy All The Time", "Set It Off", "03:39" },
    { "Wolf in Sheep's Clothing", "Set It Off", "03:00" },
    { "Partners in Crime", "Set It Off", "03:12" },
};

enum
{
    FOCUS_MENU = 0,
    FOCUS_LIST = 1,
    FOCUS_SEARCH = 2
};

static struct
{
    int nav;
    int track;
    int scroll;
    int playing;
    int focus;
    int search_edit;
    char search[SEARCH_MAX + 1];
    int exit_prompt;
    int exit_choice; /* 0 = cancelar, 1 = sair */
} app;

/* filtro atual */
static const track_info *filtered[MAX_TRACKS];
static int filtered_count = 0;

/* ultima entrada lida (usada para mostrar o estado dos dispositivos) */
static ui_input last_input;

/* layout calculado por quadro */
static f32 ui_scale = 1.0f;

/* geometria (em pixels, ja com escala) */
typedef struct
{
    f32 top_h;
    f32 side_w;
    f32 player_h;
    f32 search_x, search_y, search_w, search_h;
    f32 list_x, list_y, list_w, row_h;
    f32 nav_x, nav_y, nav_w, nav_item_h;
} metrics;

static metrics m;

static void compute_metrics(void)
{
    f32 w = (f32)gfxWidth();
    f32 h = (f32)gfxHeight();

    ui_scale = h / 720.0f;
    if (ui_scale <= 0.0f)
        ui_scale = 1.0f;

    m.top_h = 64.0f * ui_scale;
    m.side_w = 280.0f * ui_scale;
    m.player_h = 96.0f * ui_scale;

    m.search_x = 440.0f * ui_scale;
    m.search_y = 18.0f * ui_scale;
    m.search_w = w - m.search_x - 150.0f * ui_scale;
    m.search_h = 30.0f * ui_scale;

    m.list_x = m.side_w + 24.0f * ui_scale;
    m.list_y = m.top_h + 132.0f * ui_scale;
    m.list_w = w - m.list_x - 24.0f * ui_scale;
    m.row_h = 62.0f * ui_scale;

    m.nav_x = 16.0f * ui_scale;
    m.nav_y = m.top_h + 28.0f * ui_scale;
    m.nav_w = m.side_w - 32.0f * ui_scale;
    m.nav_item_h = 52.0f * ui_scale;
}

static const char *current_heading(void)
{
    return app.nav == 0 ? "Tocadas recentemente" : "Biblioteca";
}

static const char *current_subheading(void)
{
    return app.nav == 0 ? "Com base no seu historico" : "Todas as musicas";
}

/* ------------------------------------------------------------------ */
/* filtro / lista                                                     */
/* ------------------------------------------------------------------ */

static int contains_ci(const char *haystack, const char *needle)
{
    size_t nlen;
    const char *p;

    if (needle == NULL || needle[0] == '\0')
        return 1;

    nlen = strlen(needle);

    for (p = haystack; *p != '\0'; p++)
    {
        size_t i;
        int match = 1;

        for (i = 0; i < nlen; i++)
        {
            char a = p[i];
            char b = needle[i];

            if (a >= 'A' && a <= 'Z')
                a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z')
                b = (char)(b - 'A' + 'a');

            if (a != b || a == '\0')
            {
                match = 0;
                break;
            }
        }

        if (match)
            return 1;
    }

    return 0;
}

static void rebuild_filter(void)
{
    const track_info *src = (app.nav == 0) ? recent_tracks : library_tracks;
    int i;

    filtered_count = 0;

    for (i = 0; i < MAX_TRACKS; i++)
    {
        if (contains_ci(src[i].title, app.search) ||
            contains_ci(src[i].artist, app.search))
        {
            filtered[filtered_count++] = &src[i];
        }
    }

    if (app.track >= filtered_count)
        app.track = filtered_count > 0 ? filtered_count - 1 : 0;

    if (app.scroll > filtered_count - ROWS_VISIBLE)
        app.scroll = filtered_count - ROWS_VISIBLE;
    if (app.scroll < 0)
        app.scroll = 0;
}

static void clamp_scroll(void)
{
    int max_scroll = filtered_count - ROWS_VISIBLE;

    if (max_scroll < 0)
        max_scroll = 0;

    if (app.scroll > max_scroll)
        app.scroll = max_scroll;
    if (app.scroll < 0)
        app.scroll = 0;

    /* mantem a selecao visivel */
    if (app.track < app.scroll)
        app.scroll = app.track;
    if (app.track >= app.scroll + ROWS_VISIBLE)
        app.scroll = app.track - ROWS_VISIBLE + 1;
}

/* ------------------------------------------------------------------ */
/* desenho                                                            */
/* ------------------------------------------------------------------ */

static void draw_triangle_right(f32 x, f32 y, f32 w, f32 h, u32 color)
{
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

static f32 scale(void) { return ui_scale; }

static void draw_top_bar(const ui_input *in)
{
    f32 s = scale();
    f32 h = m.top_h;
    f32 logo = 22.0f * s;

    gfxRect(0, 0, (f32)gfxWidth(), h, GFX_ARGB(0xff, 0x0b, 0x0b, 0x0b));
    gfxRect(0, h - 2.0f * s, (f32)gfxWidth(), 2.0f * s, GFX_RED);

    gfxRect(24.0f * s, (h - logo) * 0.5f, logo, logo, GFX_RED);
    gfxText(24.0f * s + 3.0f * s, (h - logo) * 0.5f + 3.0f * s, 2.0f * s,
            GFX_BLACK, "M");

    gfxText(58.0f * s, (h - 24.0f * s) * 0.5f, 3.0f * s, GFX_WHITE,
            "CELL MUSIC");

    /* caixa de pesquisa */
    gfxRect(m.search_x, m.search_y, m.search_w, m.search_h,
            GFX_ARGB(0xff, 0x1a, 0x1a, 0x1a));

    if (app.focus == FOCUS_SEARCH)
        gfxRectOutline(m.search_x, m.search_y, m.search_w, m.search_h,
                       2.0f * s, GFX_RED);

    if (app.search[0] != '\0')
    {
        gfxText(m.search_x + 12.0f * s,
                m.search_y + (m.search_h - gfxTextHeight(2.0f * s)) * 0.5f,
                2.0f * s, GFX_WHITE, app.search);
    }
    else
    {
        gfxText(m.search_x + 12.0f * s,
                m.search_y + (m.search_h - gfxTextHeight(2.0f * s)) * 0.5f,
                2.0f * s, GFX_GRAY_DARK, "Pesquisar musicas, artistas");
    }

    /* cursor de texto piscando no modo de digitacao */
    if (app.search_edit && ((gfxFrameCount() / 20) % 2) == 0)
    {
        f32 caret_x = m.search_x + 12.0f * s + gfxTextWidth(app.search, 2.0f * s);

        gfxRect(caret_x, m.search_y + 6.0f * s, 2.0f * s,
                m.search_h - 12.0f * s, GFX_RED);
    }

    gfxText((f32)gfxWidth() - 92.0f * s, (h - 16.0f * s) * 0.5f, 2.0f * s,
            GFX_GRAY, "PS3");

    (void)in;
}

static void draw_sidebar(const ui_input *in)
{
    f32 s = scale();
    f32 top = m.top_h;
    f32 bottom = (f32)gfxHeight() - m.player_h;
    int i;

    gfxRect(0, top, m.side_w, bottom - top, GFX_ARGB(0xff, 0x0e, 0x0e, 0x0e));
    gfxRect(m.side_w - 2.0f * s, top, 2.0f * s, bottom - top,
            GFX_ARGB(0xff, 0x1c, 0x1c, 0x1c));

    for (i = 0; i < NAV_COUNT; i++)
    {
        f32 y = m.nav_y + (f32)i * (m.nav_item_h + 8.0f * s);
        int selected = (app.nav == i);
        int focused = selected && (app.focus == FOCUS_MENU);

        if (selected)
        {
            gfxRect(m.nav_x, y, m.nav_w, m.nav_item_h,
                    GFX_ARGB(0xff, 0x1c, 0x1c, 0x1c));
            gfxRect(m.nav_x, y, 4.0f * s, m.nav_item_h, GFX_RED);
        }

        gfxRect(m.nav_x + 24.0f * s,
                y + (m.nav_item_h - 14.0f * s) * 0.5f, 14.0f * s, 14.0f * s,
                focused ? GFX_RED : GFX_GRAY_DARK);

        gfxText(m.nav_x + 52.0f * s,
                y + (m.nav_item_h - gfxTextHeight(2.0f * s)) * 0.5f, 2.0f * s,
                focused ? GFX_WHITE : GFX_GRAY, nav_items[i]);
    }

    /* estado dos dispositivos de entrada */
    {
        f32 y = bottom - 84.0f * s;

        gfxText(m.nav_x + 8.0f * s, y, 1.5f * s,
                in->pad.connected ? GFX_GRAY : GFX_RED,
                in->pad.connected ? "CONTROLE OK" : "CONTROLE --");
        gfxText(m.nav_x + 8.0f * s, y + 18.0f * s, 1.5f * s,
                in->kb.connected ? GFX_GRAY : GFX_GRAY_DARK,
                in->kb.connected ? "TECLADO OK" : "TECLADO --");
        gfxText(m.nav_x + 8.0f * s, y + 36.0f * s, 1.5f * s,
                in->mouse.connected ? GFX_GRAY : GFX_GRAY_DARK,
                in->mouse.connected ? "MOUSE OK" : "MOUSE --");
    }

    gfxText(m.nav_x + 8.0f * s, bottom - 24.0f * s, 1.5f * s, GFX_GRAY_DARK,
            "SELECT: SAIR");
}

static void draw_list(const ui_input *in)
{
    f32 s = scale();
    int i;

    (void)in;

    gfxText(m.list_x, m.top_h + 40.0f * s, 3.0f * s, GFX_WHITE,
            current_heading());
    gfxText(m.list_x, m.top_h + 76.0f * s, 1.5f * s, GFX_GRAY,
            current_subheading());

    gfxRect(m.list_x - 8.0f * s, m.list_y - 8.0f * s, m.list_w,
            (f32)ROWS_VISIBLE * m.row_h + 16.0f * s,
            GFX_ARGB(0xff, 0x11, 0x11, 0x11));

    if (filtered_count == 0)
    {
        gfxText(m.list_x + 20.0f * s, m.list_y + 20.0f * s, 2.0f * s, GFX_GRAY,
                "Nenhum resultado");
        return;
    }

    for (i = 0; i < ROWS_VISIBLE; i++)
    {
        int index = app.scroll + i;
        f32 row_y = m.list_y + (f32)i * m.row_h;

        if (index >= filtered_count)
            break;

        {
            const track_info *t = filtered[index];
            int selected = (app.track == index);
            int playing = (app.playing == index);

            if (selected)
            {
                gfxRect(m.list_x - 8.0f * s, row_y, m.list_w, m.row_h,
                        GFX_ARGB(0xff, 0x1b, 0x1b, 0x1b));
                gfxRect(m.list_x - 8.0f * s, row_y, 4.0f * s, m.row_h,
                        (app.focus == FOCUS_LIST) ? GFX_RED : GFX_RED_DARK);
            }

            draw_thumbnail(m.list_x + 12.0f * s, row_y + 9.0f * s,
                           44.0f * s, playing);

            gfxText(m.list_x + 72.0f * s, row_y + 10.0f * s, 2.0f * s,
                    playing ? GFX_RED : GFX_WHITE, t->title);
            gfxText(m.list_x + 72.0f * s, row_y + 34.0f * s, 1.5f * s,
                    GFX_GRAY, t->artist);

            gfxText(m.list_x + m.list_w - 24.0f * s -
                        gfxTextWidth(t->duration, 1.5f * s),
                    row_y + 34.0f * s, 1.5f * s, GFX_GRAY, t->duration);
        }
    }

    /* indicador de rolagem */
    if (filtered_count > ROWS_VISIBLE)
    {
        f32 track_h = (f32)ROWS_VISIBLE * m.row_h;
        f32 bar_h = track_h * (f32)ROWS_VISIBLE / (f32)filtered_count;
        f32 bar_y = m.list_y + track_h * (f32)app.scroll / (f32)filtered_count;

        gfxRect(m.list_x + m.list_w - 6.0f * s, m.list_y, 4.0f * s, track_h,
                GFX_ARGB(0xff, 0x22, 0x22, 0x22));
        gfxRect(m.list_x + m.list_w - 6.0f * s, bar_y, 4.0f * s, bar_h,
                GFX_RED);
    }
}

static void draw_player(void)
{
    f32 s = scale();
    f32 h = m.player_h;
    f32 y = (f32)gfxHeight() - h;
    f32 bar_x = 560.0f * s;
    f32 bar_w = (f32)gfxWidth() - bar_x - 250.0f * s;
    f32 bar_y = y + 34.0f * s;
    f32 progress = 0.35f;
    f32 controls_x = (f32)gfxWidth() - 220.0f * s;
    const track_info *now;

    if (filtered_count == 0)
        now = (app.nav == 0) ? &recent_tracks[0] : &library_tracks[0];
    else
        now = filtered[app.playing < filtered_count ? app.playing : 0];

    gfxRect(0, y, (f32)gfxWidth(), h, GFX_ARGB(0xff, 0x0a, 0x0a, 0x0a));
    gfxRect(0, y, (f32)gfxWidth(), 2.0f * s, GFX_RED);

    draw_thumbnail(24.0f * s, y + 20.0f * s, 56.0f * s, 1);

    gfxText(96.0f * s, y + 26.0f * s, 2.0f * s, GFX_WHITE, now->title);
    gfxText(96.0f * s, y + 52.0f * s, 1.5f * s, GFX_GRAY, now->artist);

    gfxRect(bar_x, bar_y, bar_w, 4.0f * s, GFX_ARGB(0xff, 0x2a, 0x2a, 0x2a));
    gfxRect(bar_x, bar_y, bar_w * progress, 4.0f * s, GFX_RED);
    gfxRect(bar_x + bar_w * progress - 3.0f * s, bar_y - 2.0f * s,
            6.0f * s, 8.0f * s, GFX_WHITE);

    gfxText(bar_x, bar_y + 14.0f * s, 1.5f * s, GFX_GRAY, "02:26");
    gfxText(bar_x + bar_w - gfxTextWidth("-01:09", 1.5f * s),
            bar_y + 14.0f * s, 1.5f * s, GFX_GRAY, "-01:09");

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

static void draw_exit_prompt(void)
{
    f32 s = scale();
    f32 w = 460.0f * s;
    f32 h = 190.0f * s;
    f32 x = ((f32)gfxWidth() - w) * 0.5f;
    f32 y = ((f32)gfxHeight() - h) * 0.5f;

    gfxRect(0, 0, (f32)gfxWidth(), (f32)gfxHeight(), GFX_ARGB(0xc0, 0, 0, 0));
    gfxRect(x, y, w, h, GFX_ARGB(0xff, 0x16, 0x16, 0x16));
    gfxRectOutline(x, y, w, h, 2.0f * s, GFX_RED);

    gfxText(x + 24.0f * s, y + 24.0f * s, 3.0f * s, GFX_WHITE,
            "Sair do CELL MUSIC?");
    gfxText(x + 24.0f * s, y + 68.0f * s, 1.5f * s, GFX_GRAY,
            "O aplicativo sera encerrado.");

    /* opcoes */
    {
        const char *labels[2] = { "Cancelar", "Sair" };
        int i;

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
    }

    gfxText(x + 24.0f * s, y + 160.0f * s, 1.5f * s, GFX_GRAY_DARK,
            "X confirma   O cancela");
}

/* ------------------------------------------------------------------ */
/* desenho principal                                                  */
/* ------------------------------------------------------------------ */

void ui_draw(void)
{
    const ui_input *in = &last_input;

    compute_metrics();

    gfxFrameBegin(GFX_BLACK);

    draw_top_bar(in);
    draw_sidebar(in);
    draw_list(in);
    draw_player();

    if (app.exit_prompt)
        draw_exit_prompt();

    /* cursor do mouse sempre visivel, para mouse e teclado/controle conviverem */
    if (last_input.mouse.connected)
        draw_cursor((f32)last_input.mouse.x, (f32)last_input.mouse.y);
}

/* ------------------------------------------------------------------ */
/* interacao                                                          */
/* ------------------------------------------------------------------ */

static void move_selection(int delta)
{
    if (app.focus == FOCUS_MENU)
    {
        app.nav = (app.nav + delta + NAV_COUNT) % NAV_COUNT;
        rebuild_filter();
        app.track = 0;
        app.scroll = 0;
    }
    else if (filtered_count > 0)
    {
        app.track = (app.track + delta + filtered_count) % filtered_count;
    }

    clamp_scroll();
}

static void activate_selection(void)
{
    if (filtered_count > 0)
    {
        app.playing = app.track;
        app.focus = FOCUS_LIST;
    }
}

/* testa clique do mouse usando a mesma geometria do desenho */
static void handle_mouse_click(const ui_input *in)
{
    f32 mx = (f32)in->mouse.x;
    f32 my = (f32)in->mouse.y;
    int i;

    if (app.exit_prompt)
    {
        f32 s = scale();
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
            app.exit_choice = 0;
            return;
        }

        if (mx >= bx + 200.0f * s && mx <= bx + 380.0f * s && my >= by &&
            my <= by + 44.0f * s)
        {
            app.exit_prompt = 2; /* marcador: confirmar saida */
            return;
        }

        return;
    }

    if (mx >= m.search_x && mx <= m.search_x + m.search_w &&
        my >= m.search_y && my <= m.search_y + m.search_h)
    {
        app.focus = FOCUS_SEARCH;
        app.search_edit = 1;
        return;
    }

    for (i = 0; i < NAV_COUNT; i++)
    {
        f32 y = m.nav_y + (f32)i * (m.nav_item_h + 8.0f * scale());

        if (mx >= m.nav_x && mx <= m.nav_x + m.nav_w && my >= y &&
            my <= y + m.nav_item_h)
        {
            app.nav = i;
            app.focus = FOCUS_MENU;
            app.track = 0;
            app.scroll = 0;
            rebuild_filter();
            return;
        }
    }

    if (mx >= m.list_x - 8.0f * scale() &&
        mx <= m.list_x - 8.0f * scale() + m.list_w)
    {
        f32 row = (my - m.list_y) / m.row_h;
        int index;

        if (row < 0.0f)
            return;

        index = app.scroll + (int)row;
        if (index >= 0 && index < filtered_count)
        {
            app.focus = FOCUS_LIST;
            app.track = index;
            app.playing = index;
            clamp_scroll();
        }
    }
}

int ui_init(void)
{
    memset(&app, 0, sizeof(app));

    app.nav = 0;
    app.focus = FOCUS_LIST;
    app.track = 0;
    app.playing = 0;
    app.scroll = 0;
    app.search[0] = '\0';

    /* filtro inicial (sem pesquisa) */
    filtered_count = 0;
    {
        int i;
        const track_info *src = recent_tracks;

        for (i = 0; i < MAX_TRACKS; i++)
            filtered[filtered_count++] = &src[i];
    }

    return 0;
}

void ui_update(const ui_input *in, int *exit_requested)
{
    if (in == NULL || exit_requested == NULL)
        return;

    last_input = *in;

    /* 1) encerramento solicitado pelo teclado (ALT+F4) */
    if (in->kb.alt_f4)
    {
        *exit_requested = 1;
        return;
    }

    /* 2) overlay de saida tem prioridade */
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
            app.exit_choice = 0;
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

    /* 3) mouse */
    if (in->mouse.left)
    {
        handle_mouse_click(in);

        if (app.exit_prompt == 2)
        {
            *exit_requested = 1;
            return;
        }
    }

    if (in->mouse.wheel != 0 && filtered_count > ROWS_VISIBLE)
    {
        app.scroll += (in->mouse.wheel > 0) ? 1 : -1;
        clamp_scroll();
    }

    /* 4) pedido de saida pelo controle */
    if (in->pad.select || in->pad.start)
    {
        app.exit_prompt = 1;
        app.exit_choice = 0;
        return;
    }

    /* 5) teclado */
    if (in->kb.esc)
    {
        if (app.search_edit)
        {
            app.search_edit = 0;
        }
        else if (app.focus != FOCUS_MENU)
        {
            app.focus = FOCUS_MENU;
        }
        else
        {
            app.exit_prompt = 1;
            app.exit_choice = 0;
        }
        return;
    }

    if (in->kb.tab)
    {
        if (app.search_edit)
        {
            app.search_edit = 0;
        }
        else
        {
            app.focus = FOCUS_SEARCH;
            app.search_edit = 1;
        }
    }

    if (app.search_edit && in->kb.text_len > 0)
    {
        int i;

        for (i = 0; i < in->kb.text_len; i++)
        {
            size_t len = strlen(app.search);

            if (len < SEARCH_MAX)
            {
                app.search[len] = in->kb.text[i];
                app.search[len + 1] = '\0';
            }
        }

        rebuild_filter();
        clamp_scroll();
    }

    if (app.search_edit && in->kb.backspace)
    {
        size_t len = strlen(app.search);

        if (len > 0)
            app.search[len - 1] = '\0';

        rebuild_filter();
        clamp_scroll();
    }

    /* 6) navegacao */
    if (in->pad.up || in->kb.up)
        move_selection(-1);

    if (in->pad.down || in->kb.down)
        move_selection(1);

    if (in->pad.left || in->kb.left)
        app.focus = FOCUS_MENU;

    if (in->pad.right || in->kb.right)
        app.focus = (app.focus == FOCUS_MENU) ? FOCUS_LIST : app.focus;

    /* 7) confirmar */
    if (in->pad.cross || in->kb.enter)
        activate_selection();
}
