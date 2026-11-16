/* ui.h - modulo de interface do CELL MUSIC */
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

/* Prepara o estado da interface. Retorna 0 em caso de sucesso. */
int ui_init(void);

/* Processa entradas e atualiza o estado. Pode pedir encerramento. */
void ui_update(const ui_input *in, int *exit_requested);

/* Desenha o quadro atual (chamar entre gfxFrameBegin e gfxFrameEnd). */
void ui_draw(void);

#endif /* CELL_MUSIC_UI_H */
