CC = cc
CFLAGS = -Wall -Wextra -Werror -O2
LDFLAGS = -lfuse3

SRC = nvfs.c
BIN = nvfs

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
