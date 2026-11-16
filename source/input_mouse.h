/* input_mouse.h - mouse USB do CELL MUSIC */
#ifndef CELL_MUSIC_INPUT_MOUSE_H
#define CELL_MUSIC_INPUT_MOUSE_H

#include <ppu-types.h>

typedef struct
{
    int connected;

    int x, y; /* posicao do cursor na tela, em pixels */
    int moved;

    int left;  /* clicados na borda */
    int right;

    int wheel; /* delta de rolagem no quadro (positivo = para baixo) */
} mouse_state;

int input_mouse_init(int screen_width, int screen_height);
void input_mouse_end(void);
void input_mouse_poll(mouse_state *out);

#endif /* CELL_MUSIC_INPUT_MOUSE_H */
