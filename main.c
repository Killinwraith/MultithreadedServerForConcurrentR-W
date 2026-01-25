#include "main.h"
#include "common.h"
#include "timer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#define STR_SIZE 1000

int PushPullMessage(char *str_msg, char *str_cv, int *str_pos, int isRead);

char **theArray;
int arrayLen;
char *SERVER_IP;
long SERVER_PORT;

void deallocMem()
{
    for (int i = 0; i < arrayLen; i++)
    {
        free(theArray[i]);
    }
    free(theArray);
    free(SERVER_IP);
}

void buildArray()
{
    theArray = (char **)malloc(arrayLen * sizeof(char *));

    for (int i = 0; i < arrayLen; i++)
    {
        theArray[i] = (char *)malloc(STR_SIZE * sizeof(char));

        sprintf(theArray[i], "String [%d]: initial value", i);

        printf("%s\n", theArray[i]);
    }
}

int main(int argc, char *argv[])
{
    // double start, finish, elapsed;

    if (argc < 4)
    {
        fprintf(stderr, "Usage: %s <arrayLen> <server_ip> <server_port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

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
    printf("SERVER_PORT: %ld\n\n", SERVER_PORT);

    buildArray();

    // GET_TIME(start);

    // GET_TIME(finish);
    // elapsed = finish - start;

    deallocMem();
}