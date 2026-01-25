CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-lpthread -lm

clean:
	rm -f main
	rm -rf *.dSYM

main: clean
	$(CC) $(CFLAGS) main.c -o main 

run: main
	./main 10 127.0.0.1 5001
