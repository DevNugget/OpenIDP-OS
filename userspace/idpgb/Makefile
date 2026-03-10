.SUFFIXES:
APP := idpgb
CC ?= cc
CFLAGS := -g -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector -fno-pie -I.. -Iinclude -mno-sse -mno-avx
LDFLAGS := -nostdlib -static -no-pie -T linker.ld
SRC := idpgb.c src/cartridge.c src/emulator.c src/instruction.c src/LR35902.c src/bus.c src/process.c ../libgfx/gfx.c ../libidp/stdio.c ../libidp/window_client.c ../libidp/string.c ../libidp/type.c ../libidp/heap.c ../libidp/fs.c
OBJ := $(SRC:.c=.o)
all: $(APP).elf
$(APP).elf: $(OBJ) linker.ld
	$(CC) $(OBJ) $(LDFLAGS) -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJ) $(APP).elf
