/* input_pad.h - controle (DualShock 3 / Sixaxis) do CELL MUSIC */
#ifndef CELL_MUSIC_INPUT_PAD_H
#define CELL_MUSIC_INPUT_PAD_H

#include <ppu-types.h>

typedef struct
{
    int connected;

    /* acionados apenas na borda (acabou de pressionar) */
    int up, down, left, right;
    int cross, circle, square, triangle;
    int start, select;
    int l1, r1, l2, r2;

    int any; /* algum botao foi acionado neste quadro */
} pad_state;

/* Retorna 0 se a biblioteca de controle foi inicializada. */
int input_pad_init(void);
void input_pad_end(void);

/* Le o controle 0 e preenche os eventos de borda/repeticao. */
void input_pad_poll(pad_state *out);

#endif /* CELL_MUSIC_INPUT_PAD_H */
