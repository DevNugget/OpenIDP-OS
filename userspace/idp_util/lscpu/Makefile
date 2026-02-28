.SUFFIXES:

APP := lscpu
CC ?= cc
CFLAGS := -g -O2 -Wall -Wextra -std=gnu11 -ffreestanding -fno-stack-protector -fno-pie
LDFLAGS := -nostdlib -static -no-pie -T linker.ld

SRC := lscpu.c
OBJ := $(SRC:.c=.o)

.PHONY: all clean

all: $(APP).elf

$(APP).elf: $(OBJ) linker.ld
	$(CC) $(OBJ) $(LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(APP).elf
