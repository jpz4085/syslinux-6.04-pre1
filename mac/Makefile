## -----------------------------------------------------------------------
##
##   Copyright 2020 JPZ4085 - Based on Linux installer makefile
##   Copyright 2001-2008 H. Peter Anvin - All Rights Reserved
##
##   This program is free software; you can redistribute it and/or modify
##   it under the terms of the GNU General Public License as published by
##   the Free Software Foundation, Inc., 53 Temple Place Ste 330,
##   Boston MA 02111-1307, USA; either version 2 of the License, or
##   (at your option) any later version; incorporated herein by reference.
##
## -----------------------------------------------------------------------

##
## macOS/OSX MSDOSFS FAT installer
##

include ../mk/mac.mk

OPTFLAGS = -Oz
INCLUDES = -I../bios -I../libinstaller -I../libfat
CFLAGS	 = $(GCCWARN) -D SYSLXCOM_FOR_MAC -D USE_BACKUP_FAT $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = 

SRCS     = syslinux.c syslxopt.c syslxrw.c syslxcom.c setadv.c advio.c \
           fs.c syslxmod.c bootsect_bin.c ldlinuxc32_bin.c ldlinux_bin.c \
	   $(wildcard ../libfat/*.c)
OBJS	 = $(patsubst %.c,%.o,$(notdir $(SRCS)))

.SUFFIXES: .c .o .i .s .S

VPATH = .:../libinstaller:../libfat

all:
	$(MAKE) bootsect_bin.c ldlinux_bin.c ldlinuxc32_bin.c \
	OBJ=../bios/libinstaller SRC=. -C ../libinstaller
	$(MAKE) installer

tidy:
	-rm -f *.o *.i *.s *.a *.d *.tmp

clean: tidy
	-rm -f syslinux syslinux-nomtools
	$(MAKE) clean -C ../libinstaller

dist: tidy
	mkdir ../bios/mac
	mv syslinux syslinux-nomtools ../bios/mac/
	$(MAKE) clean -C ../libinstaller

installer: syslinux syslinux-nomtools

install:
	install -d $(BINDIR)
	install -m 755 ../bios/mac/syslinux $(BINDIR)
	install -d $(AUXDIR)
	install -m 755 ../bios/mbr/*mbr.bin $(AUXDIR)
	install -d $(MANDIR)/man1
	install -m 755 ../man/syslinux.1 $(MANDIR)/man1
	gzip -f $(MANDIR)/man1/syslinux.1

syslinux: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

syslinux-nomtools: syslinux
	ln -f $< $@

strip:
	$(STRIP) syslinux syslinux-nomtools

%.o: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -S -o $@ $<

-include .*.d
