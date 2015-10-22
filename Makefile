

PRJ=interpret

PROGS=$main $(PRJ)comp $(PRJ)
CC=gcc
CFLAGS=-Wall -std=c99 -pedantic -lm


$(PRJ): main.c global.c scanner.c string.c
	$(CC) $(CFLAGS) -o $@ main.c global.c scanner.c string.c


clean:
	rm -f *.o *.out *.exe $(PROGS)
