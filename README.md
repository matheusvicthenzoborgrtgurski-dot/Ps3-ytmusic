# YouTube Music — PlayStation 3

Aplicativo nativo para PlayStation 3 (C + PSL1GHT) que pesquisa o catálogo do
YouTube Music e reproduz áudio **no próprio console**, sem navegador e sem
depender do DeepPlay.

## O que ele faz

- Interface própria no estilo do YouTube Music para desktop: barra superior com
  pesquisa, menu lateral, área de resultados e barra de reprodução com progresso.
- **Pesquisa real** no YouTube Music (InnerTube, cliente `WEB_REMIX`) por HTTPS:
  músicas, artistas, álbuns e playlists.
- **Áudio nativo**: MP3 decodificado com libmad e enviado ao mixer do PS3
  (`cellAudio`, PCM float 48 kHz) com reamostragem 44,1 kHz → 48 kHz.
- Controle DualShock 3 (setas + X + SELECT), teclado USB (digitar, Enter, Esc,
  Alt+F4) e mouse USB com cursor próprio.
- Encerramento por caminho único: SELECT/Esc abrem o diálogo de saída, e o
  evento do XMB (`SYSUTIL_EXIT_GAME`) sempre funciona, mesmo sem periféricos.

## Por que não é uma página HTML

O PS3 homebrew não tem motor HTML acessível: os motores do console (`silk`,
`silk_webkit`) são módulos do firmware carregados apenas pelos plugins do VSH
(`webbrowser_plugin.sprx`), e o PSL1GHT não expõe nenhuma API de webview. Por
isso a interface é desenhada em RSX, mas usa as mesmas APIs que o site usa por
baixo (InnerTube), com áudio real nos alto-falantes da TV.

## Arquitetura

| Módulo | Responsabilidade |
|---|---|
| `source/gfx.c` | Vídeo RSX, superfície ARGB, flip VSYNC, texto 8x8 |
| `source/httpc.c` | HTTP/HTTPS com libcurl + mbedTLS |
| `source/ytm.c` | InnerTube (pesquisa e player) e proxy local; JSON com json-c |
| `source/audio.c` | libmad + `cellAudio` + reamostragem para 48 kHz |
| `source/input_pad.c`, `input_kb.c`, `input_mouse.c` | Controle, teclado e mouse |
| `source/ui.c` | Layout, navegação, pesquisa, cursor e diálogo de saída |
| `source/main.c` | Ordem de inicialização, eventos do sistema e encerramento |

Ordem de inicialização: vídeo → rede → áudio → dispositivos → interface.

## Rede e o proxy opcional

A pesquisa funciona direto pela internet do console. A **reprodução** depende de
uma URL de áudio tocável: o YouTube bloqueia streams sem *PO token* (BotGuard),
que exige um motor JavaScript. Com um proxy na rede local (Android/Termux com
yt-dlp + PO token) o app recebe MP3 pronto.

Configure o proxy criando `/dev_hdd0/tmp/ytm-proxy.txt` com uma linha:

```
proxy=http://192.168.0.10:8090
```

Contrato que o proxy deve atender:

- `GET /search?q=<consulta>` → JSON
  `{"tracks":[{"id":"...","title":"...","artist":"...","album":"...","duration":213,"stream":"http://..."}]}`
- `GET /stream?id=<videoId>` → corpo `audio/mpeg` (MP3)

Sem proxy, o app tenta o player InnerTube (cliente `ANDROID_VR`) e, quando o
YouTube exige PO token, mostra a mensagem pedindo o proxy.

## Compilação

```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
make
make pkg
```

Dependências vêm do `ps3dev` (toolchain + PSL1GHT) e do `ps3libraries`
(portlibs): libcurl, mbedTLS, json-c, libmad, zlib. O pacote gerado é
`cell-music.pkg` (o nome do arquivo foi mantido para preservar o workflow do
GitHub; o título instalado é **YouTube Music**).

## Como testar no console

1. Instale `cell-music.pkg` (pendrive → XMB → Install Package Files) e abra em
   **Jogo → YouTube Music**. Requer CFW ou HEN.
2. **Teclado:** TAB foca a pesquisa, digite (ex.: `daft punk`), Enter pesquisa.
   Setas navegam, Enter toca, Esc abre a saída.
3. **Controle:** setas navegam, X pesquisa/toca, SELECT e START abrem o diálogo
   de saída (X confirma, O cancela).
4. **Mouse:** mova o cursor, clique na caixa de pesquisa, no menu ou numa faixa;
   a roda rola os resultados.
5. **Sair sempre:** botão PS → Sair do jogo, ou SELECT/Esc, ou Alt+F4.

## Limitações conhecidas

- Sem áudio quando o YouTube exige PO token e não há proxy configurado.
- Capas dos álbuns não são exibidas (não há decoder de imagem embutido ainda).
- Álbuns e playlists aparecem nos resultados, mas ainda não abrem a lista de
  faixas; apenas músicas tocam.
- `mp4`/AAC (itag 140) não é decodificado: o caminho de áudio é MP3.
- Nada disso foi validado em PS3 física por quem escreveu este código; a
  validação feita foi compilação, link, empacotamento e renderização da
  interface em um shim no PC.
