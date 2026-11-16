/* main.c - CELL MUSIC (PlayStation 3, PSL1GHT)
 *
 * Inicializacao: video/RSX -> dispositivos de entrada -> interface ->
 * eventos do sistema. O encerramento passa por um unico caminho
 * (app_shutdown), usado tanto pelo pedido do usuario (controle, teclado,
 * mouse) quanto pelo evento SYSUTIL_EXIT_GAME do XMB, para que o aplicativo
 * sempre possa ser fechado sem reiniciar o console.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sysutil/sysutil.h>

#include "gfx.h"
#include "input_pad.h"
#include "input_kb.h"
#include "input_mouse.h"
#include "ui.h"

static volatile int g_sys_exit = 0;
static int g_callback_registered = 0;
static int g_pad_ok = 0;
static int g_kb_ok = 0;
static int g_mouse_ok = 0;

/* Eventos do sistema (XMB): permite sair sempre, mesmo sem perifericos. */
static void on_sysutil_event(u64 status, u64 param, void *usrdata)
{
    (void)param;
    (void)usrdata;

    switch (status)
    {
    case SYSUTIL_EXIT_GAME:
        g_sys_exit = 1;
        break;
    case SYSUTIL_DRAW_BEGIN:
    case SYSUTIL_DRAW_END:
    case SYSUTIL_MENU_OPEN:
    case SYSUTIL_MENU_CLOSE:
    default:
        break;
    }
}

static void app_shutdown(void)
{
    if (g_callback_registered)
    {
        sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
        g_callback_registered = 0;
    }

    if (g_pad_ok)
    {
        input_pad_end();
        g_pad_ok = 0;
    }

    if (g_kb_ok)
    {
        input_kb_end();
        g_kb_ok = 0;
    }

    if (g_mouse_ok)
    {
        input_mouse_end();
        g_mouse_ok = 0;
    }

    gfxShutdown();
}

int main(void)
{
    ui_input in;
    int exit_requested = 0;
    int video_ok;

    printf("CELL MUSIC: iniciando\n");

    /* 1) video */
    video_ok = (gfxInit() == 0);
    if (!video_ok)
    {
        printf("CELL MUSIC: video/RSX indisponivel, encerrando\n");
        gfxShutdown();
        return 1;
    }

    printf("CELL MUSIC: tela %ux%u\n", gfxWidth(), gfxHeight());

    /* 2) dispositivos de entrada, antes de entrar na interface */
    g_pad_ok = (input_pad_init() == 0);
    g_kb_ok = (input_kb_init() == 0);
    g_mouse_ok = (input_mouse_init((int)gfxWidth(), (int)gfxHeight()) == 0);

    if (!g_pad_ok && !g_kb_ok && !g_mouse_ok)
        printf("CELL MUSIC: nenhum dispositivo detectado; use o menu do XMB "
               "para sair\n");

    /* 3) interface */
    ui_init();

    /* 4) eventos do sistema */
    if (sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, on_sysutil_event, NULL) ==
        0)
        g_callback_registered = 1;
    else
        printf("CELL MUSIC: sysUtilRegisterCallback falhou\n");

    memset(&in, 0, sizeof(in));

    while (!exit_requested && !g_sys_exit)
    {
        /* eventos do XMB (sair, menu aberto, etc.) */
        sysUtilCheckCallback();

        input_pad_poll(&in.pad);
        input_kb_poll(&in.kb);
        input_mouse_poll(&in.mouse);

        ui_update(&in, &exit_requested);

        ui_draw(); /* ja inicia o quadro */
        gfxFrameEnd();
    }

    printf("CELL MUSIC: encerrando\n");

    app_shutdown();

    return 0;
}
