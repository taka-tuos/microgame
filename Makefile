OBJS_KERNEL = multiboot.o kernel.o memory.o console.o timer.o dsctbl.o int.o mtask.o keyboard.o fifo.o

MAKE     = make -r
GCC      = gcc
LD       = ld
DEL      = rm
COPY     = cp
GRUB     = grub-mkrescue

CFLAGS   = -O2 -m32 -nostdinc -Igolibc/

# default

default :
	$(MAKE) kernel.elf

# rules

kernel.elf : $(OBJS_KERNEL) Makefile
	$(MAKE) -C golibc
	$(LD) -m elf_i386 -Ttext=0x100000 -Map kernel.map $(OBJS_KERNEL) golibc/golibc.a -o kernel.elf

# normal rules

%.o : %.c Makefile
	$(GCC) $(CFLAGS) -c -o $*.o $*.c

%.o : %.S Makefile
	$(GCC) $(CFLAGS) -c -o $*.o $*.S

# commands

bootcd : kernel.elf
	$(COPY) kernel.elf grub/
	$(GRUB) /usr/lib/grub/i386-pc --output=bootcd.iso grub

clean :
	$(MAKE) -C golibc clean
	-$(DEL) *.o
	-$(DEL) *.elf
	-$(DEL) *.map
	-$(DEL) *.iso
