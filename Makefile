all: myos.img

boot/boot.bin: boot/boot.asm
	nasm -f bin boot/boot.asm -o boot/boot.bin

kernel/entry.o: kernel/entry.asm
	nasm -f elf32 kernel/entry.asm -o kernel/entry.o

kernel/interrupts.o: kernel/interrupts.asm
	nasm -f elf32 kernel/interrupts.asm -o kernel/interrupts.o

kernel/kernel.o: kernel/kernel.c
	x86_64-elf-gcc -m32 -ffreestanding -fno-stack-protector -nostdlib -c kernel/kernel.c -o kernel/kernel.o

kernel/memory.o: kernel/memory.c
	x86_64-elf-gcc -m32 -ffreestanding -fno-stack-protector -nostdlib -c kernel/memory.c -o kernel/memory.o

kernel/fs.o: kernel/fs.c
	x86_64-elf-gcc -m32 -ffreestanding -fno-stack-protector -nostdlib -c kernel/fs.c -o kernel/fs.o

kernel/task.o: kernel/task.c
	x86_64-elf-gcc -m32 -ffreestanding -fno-stack-protector -nostdlib -c kernel/task.c -o kernel/task.o

kernel/gfx.o: kernel/gfx.c
	x86_64-elf-gcc -m32 -ffreestanding -fno-stack-protector -nostdlib -c kernel/gfx.c -o kernel/gfx.o

kernel/kernel.bin: kernel/entry.o kernel/interrupts.o kernel/kernel.o kernel/memory.o kernel/fs.o kernel/task.o kernel/gfx.o
	x86_64-elf-ld -m elf_i386 -Ttext 0x8000 --oformat binary -o kernel/kernel.bin kernel/entry.o kernel/interrupts.o kernel/kernel.o kernel/memory.o kernel/fs.o kernel/task.o kernel/gfx.o

myos.img: boot/boot.bin kernel/kernel.bin
	cat boot/boot.bin kernel/kernel.bin > myos.img
	truncate -s 1440k myos.img

clean:
	rm -f boot/boot.bin kernel/entry.o kernel/interrupts.o kernel/kernel.o kernel/memory.o kernel/fs.o kernel/task.o kernel/gfx.o kernel/kernel.bin myos.img
