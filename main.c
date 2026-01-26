#include "main.h"
#include "common.h"
#include "timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define STR_SIZE 1000

int PushPullMessage(char *str_msg, char *str_cv, int *str_pos, int isRead);

char **theArray;
int arrayLen;
char *SERVER_IP;
long SERVER_PORT;

void deallocMem()
{
    for (int i = 0; i < arrayLen; i++)
        free(theArray[i]);
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

// [TO-DO: Add beef]
void *client_handler(void *arg);

int main(int argc, char *argv[])
{
    // double start, finish;

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

    int sockfd, connfd;
    struct sockaddr_in servaddr;
    // pthread_t pthread_handle;

    // Socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        printf("Socket creation failied\n");
        exit(0);
    }
    else
    {
        printf("Socket creation created\n");
    }
    bzero(&servaddr, sizeof(servaddr));

    // Assign IP and PORT number
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(SERVER_IP);
    servaddr.sin_port = htons(SERVER_PORT);

    // Bind the created Socket to given IP and verify
    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) >= 0)
    {
        printf("socket has been created\n");

        // Server is ready to Listen and verify
        if ((listen(sockfd, 2000)) != 0)
        {
            printf("Listen failed...\n");
            exit(0);
        }
        else
        {
            printf("Server is listening\n");

            while (1) // loop infinity
            {
                // Mutlithreaded server setup
                connfd = accept(sockfd, NULL, NULL);
                printf("Connected to client %d\n", connfd);

                // HERE IS WHERE WE NEED TO CREATE THE PTHREAD - Below is just an example...the client_handler is what we need to use for the Pthread function
                // pthread_create(*pthread_handle, NULL, client_handler, connfd);
            }
            close(sockfd);
        }
    }
    else
    {
        printf("socket creation failed\n");
    }

    // GET_TIME(start);

    // GET_TIME(finish);

    deallocMem();
}