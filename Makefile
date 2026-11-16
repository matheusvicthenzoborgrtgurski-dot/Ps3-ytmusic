# CELL MUSIC - PSL1GHT application
TARGET      := cell-music
TITLE       := CELL MUSIC
APPID       := CMUS00001
CONTENTID   := UP0001-CMUS00001_00-0000000000000001
OFILES      := source/main.o
BUILDDIR    := build
PKGFILES    := data

include $(PSL1GHT)/ppu_rules

# ps3dev/PSL1GHT instala as ferramentas do PowerPC64 PS3 com o prefixo "ppu-"
# (ppu_rules faz "PREFIX := ppu-"). Estes flags usam PS3DEV, que base_rules
# define com valor padrao quando a variavel nao esta exportada, portanto
# precisam ser expandidos depois do include.
CFLAGS      += -I$(PS3DEV)/ppu/include -std=gnu99 -O2
LDFLAGS     +=

# base_rules usa "export LD ?= $(PREFIX)gcc". O GNU make ja define LD=ld por
# padrao, entao o ?= nunca atribui e a linkagem acaba chamando o linker
# x86_64 do host sobre objetos PowerPC64 (EM: 21):
#   ld: source/main.o: Relocations in generic ELF (EM: 21)
# Forca o driver do compilador PPU (ppu-gcc) em todas as etapas de link.
LD          := $(CC)
export LD

.PHONY: pkg clean
pkg: $(TARGET).pkg

clean:
	rm -rf $(TARGET)*.pkg $(TARGET)*.self $(BUILDDIR) source/*.o
