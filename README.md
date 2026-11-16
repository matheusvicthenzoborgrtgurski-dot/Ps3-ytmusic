# CELL MUSIC — PS3

Aplicativo nativo para PlayStation 3, desenvolvido em C com PSL1GHT.

## Direção do projeto
- Interface própria inspirada na organização do YouTube Music para desktop.
- Tema escuro com detalhes vermelhos.
- Navegação por controle do PS3, teclado e mouse USB.
- Proxy HTTP executado no Android/Termux para pesquisar e fornecer faixas.
- Reprodução de áudio nativa no aplicativo (não no navegador do PS3).

## Estado atual
Esta é a base inicial do projeto. O primeiro marco é validar a cadeia de compilação PSL1GHT e gerar um pacote instalável. A interface gráfica, entrada USB, comunicação HTTP e áudio serão adicionados em etapas separadas e testadas no hardware.

## Compilação
Com o ambiente ps3dev/PSL1GHT instalado:

```sh
export PS3DEV=/usr/local/ps3dev
export PSL1GHT=$PS3DEV
export PATH=$PATH:$PS3DEV/bin:$PS3DEV/ppu/bin:$PS3DEV/spu/bin
make
make pkg
```

O pacote esperado é `cell-music.pkg`.

## Proxy
O proxy permanece separado deste repositório e continua rodando no Android com Termux. Esta base não altera nem substitui `proxy.py`.

## Marcos
1. Base nativa e pipeline de build.
2. Interface gráfica 720p e navegação.
3. Teclado/mouse USB e controle.
4. Cliente HTTP para o proxy.
5. Reprodução de áudio nativa.
6. Polimento, testes no PS3 e pacote final.

**Importante:** gerar o pacote inicial não significa que os recursos finais já estejam implementados.
