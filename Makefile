CC=gcc
CFLAGS=-g -Wall
LDFLAGS=-lpthread -lm

all: main client attacker

main: main.c common.h timer.h
	$(CC) $(CFLAGS) main.c -o main $(LDFLAGS)

client: client.c common.h
	$(CC) $(CFLAGS) client.c -o client $(LDFLAGS)

attacker: attacker.c common.h
	$(CC) $(CFLAGS) attacker.c -o attacker $(LDFLAGS)

runServer: main
	./main 10 127.0.0.1 5001

runClient: client
	./client 10 127.0.0.1 5001

runAttacker: attacker
	./attacker 10 127.0.0.1 5001

clean:
	rm -f main client attacker
	rm -rf *.dSYM
