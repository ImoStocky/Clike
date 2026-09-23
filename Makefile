CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -pedantic -g
OBJS = main.o lexal.o syntal.o str.o types.o ial.o interp.o ast.o

.PHONY: all clean

all: ifj

ifj: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) ifj
