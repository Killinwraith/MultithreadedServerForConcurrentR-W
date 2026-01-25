CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-lpthread -lm

clean:
	rm -f main

main: clean
	$(CC) $(CFLAGS) main.c -o main 