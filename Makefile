objects = main parser lexer globals predefs config
CC = gcc
CFLAGS = -Wall -Wextra
OUT ?= out

all: $(objects)

$(objects): %: %.c $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/$@ $<

OUT:
	mkdir $(OUT)

