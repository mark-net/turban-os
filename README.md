# turbanOS

A minimal 32-bit x86 operating system with keyboard input, VGA text output, and a simple interactive terminal.

## Features

- **Bootloader compliant**: Uses Multiboot standard for compatibility with GRUB and other bootloaders
- **Hardware interaction**: PS/2 keyboard driver with interrupt handling
- **Text output**: VGA 80x25 text mode with scrolling and cursor support
- **Terminal interface**: Interactive command-line with basic commands
- **Memory management**: Stack allocation and interrupt descriptor table setup
- **PIC remapping**: Properly configures the Programmable Interrupt Controller

## Commands

- `help` - Display available commands
- `clear` - Clear the terminal screen
- `fetch` - Simple echo test
- `reboot` - Display system information

## Project Structure

- `boot.asm` - Assembly boot code and interrupt setup
- `kernel.c` - Main kernel with terminal, keyboard, and VGA drivers
- `link.ld` - Linker script for kernel memory layout

## Building

Requires NASM, GCC, QEMU VM and a cross-compiler toolchain for i386. The kernel is loaded at 1MB and follows standard Multiboot conventions.
I recommend that you use microsoft WSL for compilation.
For WSL:

```bash
nasm -f elf32 boot.asm -o boot.o && gcc -m32 -ffreestanding -nostdlib -fno-stack-protector -c kernel.c -o kernel.o && ld -m elf_i386 -T link.ld -o kernel.bin boot.o kernel.o && qemu-system-i386 -kernel kernel.bin
```

## Purpose

Educational project demonstrating basic operating system concepts: boot process, hardware interaction, keyboard driver, and user interface implementation.



