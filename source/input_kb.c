/* input_kb.c - teclado USB via libio/kb do PSL1GHT
 *
 * Usa a API real: ioKbInit, ioKbGetInfo, ioKbRead e os codigos
 * KB_RAWKEY_* declarados em ppu/include/io/kb.h (codigos HID brutos).
 * Os codigos sao convertidos para ASCII por uma tabela de teclado ABNT/US
 * simples; com Shift, gera maiusculas e simbolos.
 */
#include <stdio.h>
#include <string.h>

#include <io/kb.h>

#include "input_kb.h"

#define KB_FLAGS_MASK (KB_RAWDAT | KB_KEYPAD)

static int kb_ready = 0;
static u16 previous[MAX_KEYCODES];
static int previous_count = 0;
static int previous_valid = 0;

/* Tabela HID (usage id) -> ASCII sem shift, para as teclas usadas no app */
static char hid_to_ascii(u16 code, int shift)
{
    static const char numbers[] = "1234567890";
    static const char shifted_numbers[] = "!@#$%^&*()";

    if (code >= KB_RAWKEY_A && code <= KB_RAWKEY_Z)
    {
        char c = (char)('a' + (code - KB_RAWKEY_A));
        return shift ? (char)(c - 'a' + 'A') : c;
    }

    if (code >= 0x1e && code <= 0x27) /* 1..0 */
        return shift ? shifted_numbers[code - 0x1e] : numbers[code - 0x1e];

    switch (code)
    {
    case 0x2c:
        return ' ';
    case 0x2d:
        return shift ? '_' : '-';
    case 0x2e:
        return shift ? '+' : '=';
    case 0x2f:
        return shift ? '{' : '[';
    case 0x30:
        return shift ? '}' : ']';
    case 0x31:
        return shift ? '|' : '\\';
    case 0x33:
        return shift ? ':' : ';';
    case 0x34:
        return shift ? '"' : '\'';
    case 0x35:
        return shift ? '~' : '`';
    case 0x36:
        return shift ? '<' : ',';
    case 0x37:
        return shift ? '>' : '.';
    case 0x38:
        return shift ? '?' : '/';
    default:
        return 0;
    }
}

int input_kb_init(void)
{
    memset(previous, 0, sizeof(previous));
    previous_count = 0;
    previous_valid = 0;

    if (ioKbInit(1) != 0)
    {
        printf("CELL MUSIC: ioKbInit falhou (sem teclado USB)\n");
        kb_ready = 0;
        return -1;
    }

    kb_ready = 1;
    printf("CELL MUSIC: libio/kb inicializada\n");
    return 0;
}

void input_kb_end(void)
{
    if (kb_ready)
        ioKbEnd();

    kb_ready = 0;
}

/* A tecla so conta como nova se nao estava pressionada no quadro anterior. */
static int was_pressed(u16 code)
{
    int i;

    for (i = 0; i < previous_count; i++)
    {
        if ((previous[i] & ~KB_FLAGS_MASK) == code)
            return 1;
    }

    return 0;
}

void input_kb_poll(kb_state *out)
{
    KbInfo info;
    KbData data;
    int shift, alt, ctrl;
    int i;

    if (out == NULL)
        return;

    memset(out, 0, sizeof(*out));

    if (!kb_ready)
        return;

    memset(&info, 0, sizeof(info));
    if (ioKbGetInfo(&info) != 0)
        return;

    out->connected = info.status[0] ? 1 : 0;
    if (!out->connected)
    {
        previous_valid = 0;
        previous_count = 0;
        return;
    }

    memset(&data, 0, sizeof(data));
    if (ioKbRead(0, &data) != 0)
        return;

    shift = (data.mkey._KbMkeyU._KbMkeyS.l_shift ||
             data.mkey._KbMkeyU._KbMkeyS.r_shift)
                ? 1
                : 0;
    alt = (data.mkey._KbMkeyU._KbMkeyS.l_alt ||
           data.mkey._KbMkeyU._KbMkeyS.r_alt)
              ? 1
              : 0;
    ctrl = (data.mkey._KbMkeyU._KbMkeyS.l_ctrl ||
            data.mkey._KbMkeyU._KbMkeyS.r_ctrl)
               ? 1
               : 0;

    out->alt = alt;
    out->ctrl = ctrl;

    for (i = 0; i < data.nb_keycode && i < MAX_KEYCODES; i++)
    {
        u16 raw = data.keycode[i];
        u16 code = raw & ~KB_FLAGS_MASK;
        char ch;

        if (code == 0)
            continue;

        if (previous_valid && was_pressed(code))
            continue;

        switch (code)
        {
        case KB_RAWKEY_ESCAPE:
            out->esc = 1;
            break;
        case KB_RAWKEY_UP_ARROW:
            out->up = 1;
            break;
        case KB_RAWKEY_DOWN_ARROW:
            out->down = 1;
            break;
        case KB_RAWKEY_LEFT_ARROW:
            out->left = 1;
            break;
        case KB_RAWKEY_RIGHT_ARROW:
            out->right = 1;
            break;
        case KB_RAWKEY_DELETE:
            out->del = 1;
            break;
        case KB_RAWKEY_F4:
            out->alt_f4 = alt ? 1 : 0;
            break;
        case 0x28: /* ENTER */
            out->enter = 1;
            break;
        case 0x2a: /* BACKSPACE */
            out->backspace = 1;
            break;
        case 0x2b: /* TAB */
            out->tab = 1;
            break;
        default:
            ch = hid_to_ascii(code, shift);
            if (ch != 0 && out->text_len < KB_TEXT_MAX)
                out->text[out->text_len++] = ch;
            break;
        }
    }

    memcpy(previous, data.keycode, sizeof(previous));
    previous_count = data.nb_keycode < MAX_KEYCODES ? data.nb_keycode
                                                    : MAX_KEYCODES;
    previous_valid = 1;

    out->any = out->esc || out->enter || out->backspace || out->del ||
               out->tab || out->up || out->down || out->left || out->right ||
               out->alt_f4 || out->text_len > 0;
}
