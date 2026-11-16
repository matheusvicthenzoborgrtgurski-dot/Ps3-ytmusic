/* main.c - YouTube Music para PlayStation 3 (PSL1GHT)
 *
 * Inicializacao: video/RSX -> rede (SYSMODULE_NET + libcurl) -> audio
 * (cellAudio) -> dispositivos de entrada -> interface -> eventos do sistema.
 * O encerramento passa por um unico caminho (app_shutdown), usado tanto pelo
 * pedido do usuario quanto pelo evento SYSUTIL_EXIT_GAME do XMB.
 */
#include <stdio.h>
#include <string.h>

#include <sysutil/sysutil.h>

#include "gfx.h"
#include "input_pad.h"
#include "input_kb.h"
#include "input_mouse.h"
#include "audio.h"
#include "ytm.h"
#include "ui.h"

static volatile int g_sys_exit = 0;
static int g_callback_registered = 0;
static int g_pad_ok = 0;
static int g_kb_ok = 0;
static int g_mouse_ok = 0;
static int g_audio_ok = 0;
static int g_net_ok = 0;

static void on_sysutil_event(u64 status, u64 param, void *usrdata)
{
    (void)param;
    (void)usrdata;

    if (status == SYSUTIL_EXIT_GAME)
        g_sys_exit = 1;
}

static void app_shutdown(void)
{
    if (g_callback_registered)
    {
        sysUtilUnregisterCallback(SYSUTIL_EVENT_SLOT0);
        g_callback_registered = 0;
    }

    if (g_mouse_ok)
    {
        input_mouse_end();
        g_mouse_ok = 0;
    }

    if (g_kb_ok)
    {
        input_kb_end();
        g_kb_ok = 0;
    }

    if (g_pad_ok)
    {
        input_pad_end();
        g_pad_ok = 0;
    }

    if (g_audio_ok)
    {
        audio_shutdown();
        g_audio_ok = 0;
    }

    if (g_net_ok)
    {
        ytm_shutdown();
        g_net_ok = 0;
    }

    gfxShutdown();
}

int main(void)
{
    ui_input in;
    int exit_requested = 0;

    printf("YouTube Music PS3: iniciando\n");

    if (gfxInit() != 0)
    {
        printf("YTM: video/RSX indisponivel\n");
        gfxShutdown();
        return 1;
    }

    /* rede antes da interface: a pesquisa depende dela */
    g_net_ok = (ytm_init() == 0);
    if (!g_net_ok)
        printf("YTM: rede indisponivel; a pesquisa nao vai funcionar\n");

    g_audio_ok = (audio_init() == 0);
    if (!g_audio_ok)
        printf("YTM: audio indisponivel\n");

    g_pad_ok = (input_pad_init() == 0);
    g_kb_ok = (input_kb_init() == 0);
    g_mouse_ok = (input_mouse_init((int)gfxWidth(), (int)gfxHeight()) == 0);

    ui_init();

    if (sysUtilRegisterCallback(SYSUTIL_EVENT_SLOT0, on_sysutil_event, NULL) ==
        0)
        g_callback_registered = 1;
    else
        printf("YTM: sysUtilRegisterCallback falhou\n");

    memset(&in, 0, sizeof(in));

    while (!exit_requested && !g_sys_exit)
    {
        sysUtilCheckCallback();

        input_pad_poll(&in.pad);
        input_kb_poll(&in.kb);
        input_mouse_poll(&in.mouse);

        ui_update(&in, &exit_requested);

        audio_service();

        ui_draw();
        gfxFrameEnd();
    }

    printf("YouTube Music PS3: encerrando\n");

    app_shutdown();

    return 0;
}
