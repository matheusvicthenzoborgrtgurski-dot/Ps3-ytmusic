/* input_mouse.c - mouse USB via libio/mouse do PSL1GHT
 *
 * API real: ioMouseInit, ioMouseGetInfo, ioMouseGetData (ppu/include/io/mouse.h).
 * mouseData traz x_axis/y_axis como deslocamentos relativos (s8), wheel como
 * delta e buttons como bitmask HID (bit0 = esquerdo, bit1 = direito,
 * bit2 = meio).
 */
#include <stdio.h>
#include <string.h>

#include <io/mouse.h>

#include "input_mouse.h"

#define MOUSE_BUTTON_LEFT   0x01
#define MOUSE_BUTTON_RIGHT  0x02

static int mouse_ready = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int limit_x = 0;
static int limit_y = 0;
static u8 previous_buttons = 0;
static int previous_valid = 0;

int input_mouse_init(int screen_width, int screen_height)
{
    limit_x = screen_width > 0 ? screen_width - 1 : 0;
    limit_y = screen_height > 0 ? screen_height - 1 : 0;
    cursor_x = limit_x / 2;
    cursor_y = limit_y / 2;
    previous_buttons = 0;
    previous_valid = 0;

    if (ioMouseInit(1) != 0)
    {
        printf("CELL MUSIC: ioMouseInit falhou (sem mouse USB)\n");
        mouse_ready = 0;
        return -1;
    }

    mouse_ready = 1;
    printf("CELL MUSIC: libio/mouse inicializada\n");
    return 0;
}

void input_mouse_end(void)
{
    if (mouse_ready)
        ioMouseEnd();

    mouse_ready = 0;
}

void input_mouse_poll(mouse_state *out)
{
    mouseInfo info;
    mouseData data;
    int dx, dy;

    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    out->x = cursor_x;
    out->y = cursor_y;

    if (!mouse_ready)
        return;

    memset(&info, 0, sizeof(info));
    if (ioMouseGetInfo(&info) != 0)
        return;

    out->connected = info.status[0] ? 1 : 0;
    if (!out->connected)
    {
        previous_valid = 0;
        return;
    }

    memset(&data, 0, sizeof(data));
    if (ioMouseGetData(0, &data) != 0)
        return;

    dx = (int)data.x_axis;
    dy = (int)data.y_axis;

    if (dx || dy)
    {
        cursor_x += dx;
        cursor_y += dy;

        if (cursor_x < 0)
            cursor_x = 0;
        if (cursor_y < 0)
            cursor_y = 0;
        if (cursor_x > limit_x)
            cursor_x = limit_x;
        if (cursor_y > limit_y)
            cursor_y = limit_y;

        out->moved = 1;
        out->x = cursor_x;
        out->y = cursor_y;
    }

    out->wheel = (int)data.wheel;

    if (previous_valid)
    {
        out->left = (data.buttons & MOUSE_BUTTON_LEFT) &&
                    !(previous_buttons & MOUSE_BUTTON_LEFT);
        out->right = (data.buttons & MOUSE_BUTTON_RIGHT) &&
                     !(previous_buttons & MOUSE_BUTTON_RIGHT);
    }

    previous_buttons = data.buttons;
    previous_valid = 1;
}
