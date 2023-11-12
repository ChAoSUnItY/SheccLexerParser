objects = main lexer globals predefs
CC = gcc
CFLAGS = -Wall -Wextra
OUT ?= out

all: $(objects)

$(objects): %: %.c $(OUT)
	$(CC) $(CFLAGS) -o $(OUT)/$@ $<

OUT:
	mkdir $(OUT)

