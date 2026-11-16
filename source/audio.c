/* audio.c - reproducao nativa no PS3: libmad (MP3) -> reamostragem -> cellAudio
 *
 * Fluxo: o MP3 inteiro fica em memoria, libmad decodifica quadro a quadro para
 * PCM float, o modulo reamostra para 48 kHz (taxa fixa do mixer do PS3) e
 * escreve nos blocos do buffer circular da porta de audio.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <mad.h>
#include <audio/audio.h>

#include "audio.h"

#define OUT_RATE        48000
#define OUT_CHANNELS    2
#define BLOCK_FRAMES    AUDIO_BLOCK_SAMPLES /* 256 */
#define DEC_RING_FRAMES 16384
#define OUT_RING_FRAMES 8192

/* libmad entrega ponto fixo com MAD_F_FRACBITS bits fracionarios */
static inline float mad_to_float(mad_fixed_t value)
{
    return (float)((double)value / (double)(1L << MAD_F_FRACBITS));
}

static int ready = 0;
static u32 port = 0;
static audioPortConfig cfg;
static u32 next_block = 1;

static const unsigned char *mp3_data = NULL;
static size_t mp3_size = 0;
static struct mad_stream stream;
static struct mad_frame frame;
static struct mad_synth synth;
static int decoder_open = 0;
static int decoder_eof = 0;
static int src_rate = 44100;
static int src_channels = 2;

/* anel de PCM decodificado, na taxa de origem, estéreo intercalado */
static float dec_ring[DEC_RING_FRAMES * 2];
static int dec_head = 0;   /* proximo a escrever */
static int dec_count = 0;  /* quadros disponiveis */
static double dec_pos = 0.0; /* posicao de leitura fracionaria */
static long long frames_out = 0;

static float out_ring[OUT_RING_FRAMES * 2];
static int out_count = 0;

/* ------------------------------------------------------------------ */
/* porta de audio do sistema                                          */
/* ------------------------------------------------------------------ */

int audio_init(void)
{
    audioPortParam params;
    s32 ret;

    if (ready)
        return 0;

    ret = audioInit();
    if (ret != 0)
    {
        printf("YTM: audioInit falhou (%d)\n", (int)ret);
        return -1;
    }

    params.numChannels = AUDIO_PORT_2CH;
    params.numBlocks = AUDIO_BLOCK_8;
    params.attrib = AUDIO_PORT_INITLEVEL;
    params.level = 1.0f;

    ret = audioPortOpen(&params, &port);
    if (ret != 0)
    {
        printf("YTM: audioPortOpen falhou (%d)\n", (int)ret);
        audioQuit();
        return -1;
    }

    ret = audioGetPortConfig(port, &cfg);
    if (ret != 0)
    {
        printf("YTM: audioGetPortConfig falhou (%d)\n", (int)ret);
        audioPortClose(port);
        audioQuit();
        return -1;
    }

    ret = audioPortStart(port);
    if (ret != 0)
    {
        printf("YTM: audioPortStart falhou (%d)\n", (int)ret);
        audioPortClose(port);
        audioQuit();
        return -1;
    }

    next_block = 1;
    ready = 1;
    printf("YTM: audio %d canais, %d blocos, %d bytes\n", (int)cfg.channelCount,
           (int)cfg.numBlocks, (int)cfg.portSize);

    return 0;
}

void audio_shutdown(void)
{
    if (!ready)
        return;

    audioPortStop(port);
    audioPortClose(port);
    audioQuit();

    ready = 0;
    decoder_open = 0;
}

int audio_ready(void) { return ready; }

/* ------------------------------------------------------------------ */
/* decodificacao                                                      */
/* ------------------------------------------------------------------ */

static void push_dec_frame(float left, float right)
{
    int slot = dec_head;

    dec_ring[slot * 2 + 0] = left;
    dec_ring[slot * 2 + 1] = right;

    dec_head = (dec_head + 1) % DEC_RING_FRAMES;
    if (dec_count < DEC_RING_FRAMES)
        dec_count++;
    else
        dec_pos -= 1.0; /* descartou o mais antigo */
}

static float dec_get(int back)
{
    /* back = 0 -> amostra mais antiga ainda nao consumida */
    int idx;

    if (back >= dec_count)
        return 0.0f;

    idx = (dec_head - dec_count + back + DEC_RING_FRAMES * 2) % DEC_RING_FRAMES;

    return dec_ring[idx * 2];
}

static float dec_get_r(int back)
{
    int idx;

    if (back >= dec_count)
        return 0.0f;

    idx = (dec_head - dec_count + back + DEC_RING_FRAMES * 2) % DEC_RING_FRAMES;

    return dec_ring[idx * 2 + 1];
}

static void consume_dec(int frames)
{
    if (frames > dec_count)
        frames = dec_count;

    dec_count -= frames;
}

/* Decodifica um quadro MAD e empilha no anel de origem. Retorna 1 se produziu. */
static int decode_frame(void)
{
    int guard = 0;

    while (!decoder_eof && guard < 64)
    {
        int err;

        guard++;

        err = mad_frame_decode(&frame, &stream);

        if (err != 0)
        {
            if (MAD_RECOVERABLE(stream.error))
                continue;

            if (stream.error == MAD_ERROR_BUFLEN)
            {
                decoder_eof = 1;
                return 0;
            }

            continue;
        }

        mad_synth_frame(&synth, &frame);

        src_rate = (int)synth.pcm.samplerate;
        src_channels = synth.pcm.channels;

        if (src_rate <= 0)
            src_rate = 44100;

        {
            unsigned int i;

            for (i = 0; i < synth.pcm.length; i++)
            {
                float l = mad_to_float(synth.pcm.samples[0][i]);
                float r = src_channels > 1
                              ? mad_to_float(synth.pcm.samples[1][i])
                              : l;

                /* evita estourar o mixer */
                if (l > 1.0f)
                    l = 1.0f;
                if (l < -1.0f)
                    l = -1.0f;
                if (r > 1.0f)
                    r = 1.0f;
                if (r < -1.0f)
                    r = -1.0f;

                push_dec_frame(l, r);
            }
        }

        /* guarda a duracao aproximada para a barra de progresso */
        frames_out += synth.pcm.length;

        return 1;
    }

    return 0;
}

/* Reamostra do anel de origem para 48 kHz, enchendo o anel de saida. */
static void resample_into_out(void)
{
    double step = (double)src_rate / (double)OUT_RATE;

    while (out_count < OUT_RING_FRAMES)
    {
        int base = (int)dec_pos;
        double frac;
        float l, r;

        if (dec_count < base + 2)
        {
            if (!decode_frame())
                break;
            continue;
        }

        frac = dec_pos - (double)base;

        l = dec_get(base) * (float)(1.0 - frac) +
            dec_get(base + 1) * (float)frac;
        r = dec_get_r(base) * (float)(1.0 - frac) +
            dec_get_r(base + 1) * (float)frac;

        out_ring[out_count * 2 + 0] = l;
        out_ring[out_count * 2 + 1] = r;
        out_count++;

        dec_pos += step;

        {
            int drop = (int)dec_pos;

            if (drop > 0)
            {
                consume_dec(drop);
                dec_pos -= (double)drop;
            }
        }
    }
}

static int out_shift(int frames)
{
    if (frames > out_count)
        frames = out_count;

    if (frames <= 0)
        return 0;

    memmove(out_ring, out_ring + frames * 2,
            (size_t)(out_count - frames) * 2 * sizeof(float));
    out_count -= frames;

    return frames;
}

/* ------------------------------------------------------------------ */
/* escrita na porta de audio                                          */
/* ------------------------------------------------------------------ */

static u32 hw_read_index(void)
{
    return *(volatile u32 *)&cfg.readIndex;
}

static int write_one_block(void)
{
    float *dst;
    u32 read = hw_read_index();
    int i;

    if (!ready || !cfg.audioDataStart)
        return 0;

    if (read == next_block)
        return 0; /* o hardware ainda nao liberou este bloco */

    if (out_count < BLOCK_FRAMES)
        return 0;

    dst = (float *)(u64)cfg.audioDataStart +
          (size_t)OUT_CHANNELS * BLOCK_FRAMES * next_block;

    for (i = 0; i < BLOCK_FRAMES * OUT_CHANNELS; i++)
        dst[i] = out_ring[i];

    out_shift(BLOCK_FRAMES);

    next_block = (next_block + 1) % cfg.numBlocks;

    return 1;
}

/* ------------------------------------------------------------------ */
/* API publica                                                        */
/* ------------------------------------------------------------------ */

int audio_load_mp3(const unsigned char *data, size_t size)
{
    if (!ready || data == NULL || size == 0)
        return -1;

    audio_stop();

    if (decoder_open)
    {
        mad_frame_finish(&frame);
        mad_stream_finish(&stream);
        decoder_open = 0;
    }

    mp3_data = data;
    mp3_size = size;

    mad_stream_init(&stream);
    mad_frame_init(&frame);
    mad_synth_init(&synth);

    mad_stream_buffer(&stream, (unsigned char *)mp3_data, mp3_size);

    decoder_open = 1;
    decoder_eof = 0;
    dec_head = 0;
    dec_count = 0;
    dec_pos = 0.0;
    out_count = 0;
    frames_out = 0;
    src_rate = 44100;
    src_channels = 2;
    next_block = (hw_read_index() + 2) % AUDIO_BLOCK_8;

    /* preenche um pouco antes de comecar para evitar silencio no inicio */
    resample_into_out();

    return 0;
}

void audio_service(void)
{
    int guard = 0;

    if (!ready || !decoder_open)
        return;

    while (guard < 8)
    {
        if (out_count < BLOCK_FRAMES * 3)
            resample_into_out();

        if (!write_one_block())
            break;

        guard++;
    }
}

void audio_play(void)
{
    /* a porta ja esta iniciada; paramos apenas de alimentar ao pausar */
}

void audio_pause(void) {}

int audio_is_playing(void) { return ready && decoder_open && !decoder_eof; }

int audio_position(void) { return (int)(frames_out / OUT_RATE); }

int audio_duration(void)
{
    if (mp3_size == 0 || src_rate <= 0)
        return 0;

    /* estimativa pelo bitrate medio do arquivo (128 kbps) */
    return (int)((double)mp3_size * 8.0 / 128000.0);
}

void audio_stop(void)
{
    if (!decoder_open)
        return;

    mad_frame_finish(&frame);
    mad_stream_finish(&stream);

    decoder_open = 0;
    decoder_eof = 0;
    dec_count = 0;
    out_count = 0;

    mp3_data = NULL;
    mp3_size = 0;
}
