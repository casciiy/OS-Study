#
# Makefile for linux.
# If you don't have '-mstring-insns' in your gcc (and nobody but me has :-)
# remove them from the CFLAGS defines.
#

AS86	=as86 -0 -a
CC86	=cc -0
LD86	=ld86 -0

AS	=as
LD	=ld
LDFLAGS	= -m elf_i386 -Ttext 0 -e startup_32
CC	=gcc $(RAMDISK)
CFLAGS	=-Wall -O2 -fomit-frame-pointer
CPP	=cpp -nostdinc -Iinclude

ARCHIVES=kernel/kernel.o mm/mm.o fs/fs.o
LIBS	=lib/lib.a
.c.s:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -S -o $*.s $<
.s.o:
	$(AS) -o $*.o $<
.c.o:
	$(CC) $(CFLAGS) \
	-nostdinc -Iinclude -c -o $*.o $<

all:	Image

Image: boot/boot tools/system tools/build
	objcopy -O binary -R .note -R .comment tools/system tools/kernel
	tools/build boot/boot tools/kernel > Image
	./ndisasm -b32 tools/kernel > System.asm
	rm tools/kernel -f
	sync

disk:Image
	dd if=Image of=/dev/fd0 bs=8192
tools/build: tools/build.c
	$(CC) $(CFLAGS) \
	-o tools/build tools/build.c
	

boot/head.o: boot/head.s

	gcc -D__KERNEL__ -I./include  -traditional -c boot/head.s
	mv head.o boot/
	

tools/system:	boot/head.o init/main.o \
		$(ARCHIVES) $(LIBS)
	$(LD) $(LDFLAGS) boot/head.o init/main.o \
	$(ARCHIVES) \
	$(LIBS) \
	-o tools/system  
	nm tools/system | grep -v '\(compiled\)\|\(\.o$$\)\|\( [aU] \)\|\(\.\.ng$$\)\|\(LASH[RL]DI\)'| sort > System.map


kernel/kernel.o:
	(cd kernel; make)

mm/mm.o:
	(cd mm; make)

fs/fs.o:
	(cd fs; make)

lib/lib.a:
	(cd lib; make)

boot/boot:	boot/boot.o tools/system
	$(LD86) -s -o boot/boot boot/boot.o
boot/boot.o:    boot/boot.s
	$(AS86) -o $@ $<

clean:
	rm -f Image System.map tmp_make boot/boot core
	rm -f init/*.o boot/*.o tools/system System.asm 
	(cd mm;make clean)
	(cd fs;make clean)
	(cd kernel;make clean)
	(cd lib;make clean)

backup: clean
	(cd .. ; tar cf - linux | compress16 - > backup.Z)
	sync

dep:
	sed '/\#\#\# Dependencies/q' < Makefile > tmp_make
	(for i in init/*.c;do echo -n "init/";$(CPP) -M $$i;done) >> tmp_make
	cp tmp_make Makefile
	(cd fs; make dep)
	(cd kernel; make dep)
	(cd mm; make dep)

### Dependencies:
init/main.o : init/main.c include/unistd.h include/sys/stat.h \
  include/sys/types.h include/sys/times.h include/sys/utsname.h \
  include/utime.h include/time.h include/linux/tty.h include/termios.h \
  include/linux/sched.h include/linux/head.h include/linux/fs.h \
  include/linux/mm.h include/asm/system.h include/asm/io.h include/stddef.h \
  include/stdarg.h include/fcntl.h 
