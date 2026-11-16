/* input_kb.h - teclado USB do CELL MUSIC */
#ifndef CELL_MUSIC_INPUT_KB_H
#define CELL_MUSIC_INPUT_KB_H

#include <ppu-types.h>

#define KB_TEXT_MAX 8

typedef struct
{
    int connected;

    /* teclas especiais: acionadas na borda */
    int esc, enter, backspace, del, tab;
    int up, down, left, right;
    int alt, ctrl;
    int alt_f4; /* pedido de encerramento pelo teclado */

    /* caracteres digitados neste quadro (ASCII) */
    char text[KB_TEXT_MAX];
    int text_len;

    int any;
} kb_state;

int input_kb_init(void);
void input_kb_end(void);
void input_kb_poll(kb_state *out);

#endif /* CELL_MUSIC_INPUT_KB_H */
