include ../mk/mac.mk

OPTFLAGS = -Oz
INCLUDES = -I../bios -I../libinstaller -I../libfat
CFLAGS	 = $(GCCWARN) $(OPTFLAGS) $(INCLUDES)
LDFLAGS	 = 

SRCS     = syslinux.c fs.c syslxmod.c syslxopt.c syslxrw.c setadv.c \
	   bootsect_bin.c ldlinux_bin.c ldlinuxc32_bin.c \
	   $(sort $(wildcard ../libfat/*.c))
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
	-rm -f syslinux
	$(MAKE) clean -C ../libinstaller

dist: tidy
	mkdir ../bios/mtools-mac
	mv syslinux ../bios/mtools-mac
	$(MAKE) clean -C ../libinstaller

installer: syslinux

install:
	install -d $(BINDIR)
	install -m 755 ../bios/mtools-mac/syslinux $(BINDIR)
	install -d $(AUXDIR)
	install -m 755 ../bios/mbr/*mbr.bin $(AUXDIR)
	install -d $(MANDIR)/man1
	install -m 755 ../man/syslinux.1 $(MANDIR)/man1
	gzip -f $(MANDIR)/man1/syslinux.1

syslinux: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

strip:
	$(STRIP) syslinux

%.o: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -c -o $@ $<
%.i: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -E -o $@ $<
%.s: %.c
	$(CC) $(UMAKEDEPS) $(CFLAGS) -S -o $@ $<

-include .*.d
