# CELL MUSIC - PSL1GHT application
TARGET      := cell-music
TITLE       := CELL MUSIC
APPID       := CMUS00001
CONTENTID   := UP0001-CMUS00001_00-0000000000000001
OFILES      := source/main.o
CFLAGS      := -I$(PS3DEV)/ppu/include -std=gnu99 -O2
LDFLAGS     :=
BUILDDIR    := build
PKGFILES    := data

include $(PSL1GHT)/ppu_rules

.PHONY: pkg clean
pkg: $(TARGET).pkg

clean:
	rm -rf $(TARGET)*.pkg $(TARGET)*.self $(BUILDDIR) source/*.o
