#include "main.h"
#include "common.h"
#include "timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>

int PushPullMessage(char *str_msg, char *str_cv, int *str_pos, int isRead);

char **theArray;
int arrayLen;
char *SERVER_IP;
long SERVER_PORT;

void deallocMem()
{
    free(theArray);
}

int main(int argc, char *argv[])
{
    // double start, finish, elapsed;

    SERVER_IP = malloc(strlen(argv[2]) + 1);
    if (!SERVER_IP)
    {
        perror("malloc SERVER_IP");
        exit(EXIT_FAILURE);
    }
    strcpy(SERVER_IP, argv[2]);

    arrayLen = atoi(argv[1]);
    SERVER_PORT = atoi(argv[3]);

    printf("ArrayLen: %d\n", arrayLen);
    printf("SERVER_IP: %s\n", SERVER_IP);
    printf("SERVER_PORT: %ld\n", SERVER_PORT);

    // GET_TIME(start);

    // GET_TIME(finish);
    // elapsed = finish - start;
}