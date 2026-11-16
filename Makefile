# YouTube Music PS3 - aplicativo PSL1GHT
# O nome do arquivo continua cell-music.* para preservar o workflow do GitHub
# (que publica cell-music.pkg); o titulo instalado e "YouTube Music".
TARGET      := cell-music
TITLE       := YouTube Music
APPID       := YTM000001
CONTENTID   := UP0001-YTM000001_00-0000000000000001
BUILDDIR    := build
PKGFILES    := data

# OFILES precisa ser definido antes do include: a regra %.elf de base_rules
# usa $(OFILES) como pre-requisito no momento em que e lida.
OFILES      := source/main.o \
               source/gfx.o \
               source/ui.o \
               source/audio.o \
               source/httpc.o \
               source/ytm.o \
               source/input_pad.o \
               source/input_kb.o \
               source/input_mouse.o

include $(PSL1GHT)/ppu_rules

# Bibliotecas portadas (ps3dev/portlibs): libcurl+mbedTLS, json-c, libmad, zlib
PORTLIBS    := $(PS3DEV)/portlibs/ppu

# Estes flags usam PS3DEV, definido por base_rules quando nao esta exportado,
# portanto precisam ser expandidos depois do include.
CFLAGS      += $(LIBPSL1GHT_INC) -I$(PORTLIBS)/include -std=gnu99 -O2 -Wall \
               -mcpu=cell $(MACHDEP)
LDFLAGS     += $(MACHDEP)
LIBPATHS    := $(LIBPSL1GHT_LIB) -L$(PORTLIBS)/lib

# Ordem importa: curl depende de mbedTLS/zlib/net, e o restante e PSL1GHT.
LIBS        := -lcurl -lmbedtls -lmbedx509 -lmbedcrypto -ljson-c -lmad -lz \
               -laudio -lnet -lsysmodule -lsysutil -lrsx -lgcm_sys -lio -lrt \
               -llv2 -lm

# base_rules usa "export LD ?= $(PREFIX)gcc". O GNU make ja define LD=ld por
# padrao, entao o ?= nunca atribui e a linkagem acabava chamando o linker
# x86_64 do host sobre objetos PowerPC64 (EM: 21).
LD          := $(CC)
export LD

.PHONY: pkg clean
pkg: $(TARGET).pkg

clean:
	rm -rf $(TARGET)*.pkg $(TARGET)*.self $(BUILDDIR) source/*.o
