/* input_pad.c - leitura do controle PS3 (libio/pad do PSL1GHT)
 *
 * Usa apenas a API real da toolchain: ioPadInit, ioPadGetInfo, ioPadGetData
 * e os campos BTN_* de padData (ver ppu/include/io/pad.h).
 *
 * Repeticao: setas seguradas geram eventos depois de um intervalo e em
 * seguida a cada poucos quadros, para navegar em listas longas.
 */
#include <stdio.h>
#include <string.h>

#include <io/pad.h>

#include "input_pad.h"

#define REPEAT_DELAY_FRAMES  16
#define REPEAT_EVERY_FRAMES  5

static int pad_ready = 0;
static padData previous;
static int have_previous = 0;

static int hold_up = 0, hold_down = 0, hold_left = 0, hold_right = 0;

int input_pad_init(void)
{
    previous.BTN_UP = previous.BTN_DOWN = 0;
    previous.BTN_LEFT = previous.BTN_RIGHT = 0;
    hold_up = hold_down = hold_left = hold_right = 0;
    have_previous = 0;

    if (ioPadInit(1) != 0)
    {
        printf("CELL MUSIC: ioPadInit falhou (sem controle)\n");
        pad_ready = 0;
        return -1;
    }

    pad_ready = 1;
    printf("CELL MUSIC: libio/pad inicializada\n");
    return 0;
}

void input_pad_end(void)
{
    if (pad_ready)
        ioPadEnd();

    pad_ready = 0;
}

static int repeat_flag(int held, int *counter)
{
    int fire = 0;

    if (!held)
    {
        *counter = 0;
        return 0;
    }

    if (*counter == 0)
        fire = 1; /* borda */
    else if (*counter >= REPEAT_DELAY_FRAMES &&
             ((*counter - REPEAT_DELAY_FRAMES) % REPEAT_EVERY_FRAMES) == 0)
        fire = 1;

    (*counter)++;
    return fire;
}

void input_pad_poll(pad_state *out)
{
    padInfo info;
    padData data;
    int connected = 0;

    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (!pad_ready)
        return;

    memset(&info, 0, sizeof(info));
    if (ioPadGetInfo(&info) != 0)
        return;

    connected = info.status[0] ? 1 : 0;
    out->connected = connected;

    if (!connected)
    {
        /* desconexao: zera o estado para nao repetir eventos antigos */
        have_previous = 0;
        hold_up = hold_down = hold_left = hold_right = 0;
        return;
    }

    memset(&data, 0, sizeof(data));
    if (ioPadGetData(0, &data) != 0)
        return;

    /* setas com repeticao */
    out->up = repeat_flag(data.BTN_UP ? 1 : 0, &hold_up);
    out->down = repeat_flag(data.BTN_DOWN ? 1 : 0, &hold_down);
    out->left = repeat_flag(data.BTN_LEFT ? 1 : 0, &hold_left);
    out->right = repeat_flag(data.BTN_RIGHT ? 1 : 0, &hold_right);

    /* botoes de acao: somente borda */
    if (have_previous)
    {
        out->cross = (data.BTN_CROSS && !previous.BTN_CROSS);
        out->circle = (data.BTN_CIRCLE && !previous.BTN_CIRCLE);
        out->square = (data.BTN_SQUARE && !previous.BTN_SQUARE);
        out->triangle = (data.BTN_TRIANGLE && !previous.BTN_TRIANGLE);
        out->start = (data.BTN_START && !previous.BTN_START);
        out->select = (data.BTN_SELECT && !previous.BTN_SELECT);
        out->l1 = (data.BTN_L1 && !previous.BTN_L1);
        out->r1 = (data.BTN_R1 && !previous.BTN_R1);
        out->l2 = (data.BTN_L2 && !previous.BTN_L2);
        out->r2 = (data.BTN_R2 && !previous.BTN_R2);
    }

    previous = data;
    have_previous = 1;

    out->any = out->up || out->down || out->left || out->right ||
               out->cross || out->circle || out->square || out->triangle ||
               out->start || out->select || out->l1 || out->r1 ||
               out->l2 || out->r2;
}
