/* ui.h - interface do YouTube Music para PS3 */
#ifndef CELL_MUSIC_UI_H
#define CELL_MUSIC_UI_H

#include "gfx.h"
#include "input_pad.h"
#include "input_kb.h"
#include "input_mouse.h"

typedef struct
{
    pad_state pad;
    kb_state kb;
    mouse_state mouse;
} ui_input;

int ui_init(void);

/* Processa as entradas; pode pedir encerramento (1) ou nao (0). */
void ui_update(const ui_input *in, int *exit_requested);

/* Desenha o quadro (chamar entre gfxFrameBegin e gfxFrameEnd). */
void ui_draw(void);

#endif /* CELL_MUSIC_UI_H */
