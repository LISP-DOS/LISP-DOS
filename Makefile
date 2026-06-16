CC = gcc
LD = ld

CFLAGS = -Wall -Wextra -O2 -pipe -fno-builtin -fno-stack-protector -ffreestanding -m64 -mno-80387 -mno-mmx -mno-sse -mno-sse2 -mno-red-zone -mcmodel=kernel -fno-pic -fno-pie -I.
LDFLAGS = -nostdlib -z max-page-size=0x1000 -static -no-pie -T linker.ld

SRCS = src/kernel/main.c src/hal/vga.c src/hal/gdt.c src/hal/idt.c src/hal/pic.c src/hal/keyboard.c src/kernel/pmm.c src/kernel/vmm.c src/kernel/heap.c src/kernel/tarfs.c src/hal/ata.c src/kernel/fat16.c src/hal/acpi.c src/hal/pci.c src/hal/usb.c src/hal/serial.c src/kernel/string.c src/kernel/syscall.c src/kernel/process.c
OBJS = $(SRCS:.c=.o) src/hal/interrupts.o src/kernel/syscall_asm.o

.PHONY: all clean run limine

all: msdos-lisp.iso hdd.img

limine:
	@if [ ! -d "limine" ]; then git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1; fi
	@if [ ! -f "limine.h" ]; then curl -Lo limine.h https://raw.githubusercontent.com/limine-bootloader/limine/v8.x-binary/limine.h; fi
	make -C limine

ramdisk.tar: editor.elf lisp.elf test.elf
	rm -rf ramdisk
	mkdir -p ramdisk/kernel
	mkdir -p ramdisk/boot
	mkdir -p ramdisk/bin
	mkdir -p ramdisk/lib
	cp editor.elf ramdisk/bin/
	cp lisp.elf ramdisk/bin/
	cp test.elf ramdisk/bin/
	cd ramdisk && tar -cvf ../ramdisk.tar *

hdd.img: test.elf editor.elf lisp.elf
	@if [ ! -f "hdd.img" ]; then \
		dd if=/dev/zero of=hdd.img bs=1M count=10; \
		mkfs.fat -F 16 hdd.img; \
	fi
	mcopy -o -i hdd.img test.elf ::/test.elf
	mcopy -o -i hdd.img editor.elf ::/editor.elf
	mcopy -o -i hdd.img lisp.elf ::/lisp.elf
	mcopy -o -i hdd.img src/tools/lisp.txt ::/lisp.txt

test.elf: src/test/test.c
	$(CC) -Wall -Wextra -O2 -fno-builtin -fno-stack-protector -ffreestanding -m64 -fno-pic -fno-pie -no-pie -nostdlib src/test/test.c -o test.elf

editor.elf: src/tools/editor.c
	$(CC) -Wall -Wextra -O2 -fno-builtin -fno-stack-protector -ffreestanding -m64 -fno-pic -fno-pie -no-pie -nostdlib src/tools/editor.c -o editor.elf

lisp.elf: src/tools/lisp.c
	$(CC) -Wall -Wextra -O2 -fno-builtin -fno-stack-protector -ffreestanding -m64 -fno-pic -fno-pie -no-pie -nostdlib src/tools/lisp.c -o lisp.elf

kernel.elf: limine $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

msdos-lisp.iso: kernel.elf limine ramdisk.tar
	rm -rf iso_root
	mkdir -p iso_root/kernel
	mkdir -p iso_root/boot
	mkdir -p iso_root/bin
	mkdir -p iso_root/lib
	cp kernel.elf iso_root/kernel/
	cp limine.conf iso_root/
	cp ramdisk.tar iso_root/boot/
	cp limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin iso_root/boot/
	mkdir -p iso_root/EFI/BOOT
	cp limine/BOOTX64.EFI iso_root/EFI/BOOT/
	cp limine/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -b boot/limine-bios-cd.bin -no-emul-boot -boot-load-size 4 -boot-info-table --efi-boot boot/limine-uefi-cd.bin -efi-boot-part --efi-boot-image --protective-msdos-label iso_root -o lisp-dos.iso
	./limine/limine bios-install lisp-dos.iso

run: all
	qemu-system-x86_64 -m 256M -cdrom lisp-dos.iso -drive file=hdd.img,format=raw,if=ide -boot d -serial stdio -d int -D qemu.log

clean:
	rm -f $(OBJS) kernel.elf lisp-dos.iso
	rm -rf iso_root test.elf editor.elf lisp.elf
