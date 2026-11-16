/* audio.h - reproducao de audio nativa no PS3 (cellAudio + libmad)
 *
 * O PS3 so toca PCM float 32 bits a 48 kHz; o decoder libmad entrega MP3 a
 * 44.1/48 kHz, entao o modulo reamostra para 48 kHz antes de escrever no
 * buffer circular da porta de audio (audio/audio.h do PSL1GHT).
 */
#ifndef CELL_MUSIC_AUDIO_H
#define CELL_MUSIC_AUDIO_H

#include <ppu-types.h>

/* Abre a porta de audio do sistema. Retorna 0 em sucesso. */
int audio_init(void);
void audio_shutdown(void);

int audio_ready(void);

/* Entrega um MP3 completo em memoria para reproducao. Retorna 0 em sucesso. */
int audio_load_mp3(const unsigned char *data, size_t size);

/* Alimenta a porta de audio com PCM decodificado. Chamar a cada quadro. */
void audio_service(void);

void audio_play(void);
void audio_pause(void);
int audio_is_playing(void);

/* Posicao atual e duracao em segundos. */
int audio_position(void);
int audio_duration(void);

void audio_stop(void);

#endif /* CELL_MUSIC_AUDIO_H */
