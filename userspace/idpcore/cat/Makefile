.SUFFIXES:
APP := cat
CC ?= cc
CFLAGS := -g -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector -fno-pie -I../.. -mno-sse -mno-avx
LDFLAGS := -nostdlib -static -no-pie -T linker.ld
SRC := cat.c ../../libidp/stdio.c
OBJ := $(SRC:.c=.o)
all: $(APP).elf
$(APP).elf: $(OBJ) linker.ld
	$(CC) $(OBJ) $(LDFLAGS) -o $@
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
clean:
	rm -f $(OBJ) $(APP).elf
