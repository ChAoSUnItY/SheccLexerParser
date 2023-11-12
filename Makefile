objects = main lexer predefs
CC = gcc
CFLAGS = -Wall
OUT ?= out

all: $(objects)

$(objects): %: %.c OUT
	$(CC) $(CFLAGS) -o OUT/$@ $<

OUT:
	mkdir OUT

