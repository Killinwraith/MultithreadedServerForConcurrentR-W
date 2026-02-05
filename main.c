#include "main.h"
#include "common.h"
#include "timer.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define STR_SIZE 1000

sem_t thread_sem;

int PushPullMessage(char *str_msg, char *str_cv, int *str_pos, int isRead);

char **theArray;
int arrayLen;
char *SERVER_IP;
long SERVER_PORT;

typedef struct
{
    pthread_mutex_t mutex;
    double latencies[COM_NUM_REQUEST];
    int latency_num;
} latency_mutex_t;

latency_mutex_t latency_mutex;

void init_latency_mutex(latency_mutex_t *latency_mutex)
{
    latency_mutex->latency_num = 0;
    pthread_mutex_init(&latency_mutex->mutex, NULL);
}

typedef struct
{
    pthread_mutex_t mutex;
    pthread_cond_t readers_active_cond;
    pthread_cond_t writers_active_cond;
    int readers_active;
    int writers_active;
    int pending_writers;
} rw_mutex_t;

rw_mutex_t rw_mutex;

void init_rw_mutex(rw_mutex_t *rw_mutex)
{
    rw_mutex->readers_active = 0;
    rw_mutex->writers_active = 0;
    rw_mutex->pending_writers = 0;
    pthread_mutex_init(&rw_mutex->mutex, NULL);
    pthread_cond_init(&rw_mutex->readers_active_cond, NULL);
    pthread_cond_init(&rw_mutex->writers_active_cond, NULL);
}

// create a write lock
void rw_writer_lock(rw_mutex_t *rw_mutex)
{
    pthread_mutex_lock(&rw_mutex->mutex);
    rw_mutex->pending_writers++;

    while (rw_mutex->readers_active > 0 || rw_mutex->writers_active > 0)
        pthread_cond_wait(&rw_mutex->writers_active_cond, &rw_mutex->mutex); // wait until there is no active reader or writer

    rw_mutex->writers_active++;
    rw_mutex->pending_writers--;
    pthread_mutex_unlock(&rw_mutex->mutex);
}

// create the reader lock
void rw_reader_lock(rw_mutex_t *rw_mutex)
{
    pthread_mutex_lock(&rw_mutex->mutex);
    while (rw_mutex->writers_active > 0 || rw_mutex->pending_writers > 0)
        pthread_cond_wait(&rw_mutex->readers_active_cond, &rw_mutex->mutex); // wait until there is no active writer or pending writer

    rw_mutex->readers_active++;
    pthread_mutex_unlock(&rw_mutex->mutex);
}

// unlock function for both reader and writer
void rw_unlock(rw_mutex_t *rw_mutex)
{
    pthread_mutex_lock(&rw_mutex->mutex);
    if (rw_mutex->writers_active > 0)
    {
        rw_mutex->writers_active--;
    }
    else if (rw_mutex->readers_active > 0)
    {
        rw_mutex->readers_active--;
    }
    if (rw_mutex->pending_writers > 0)
    {
        if (rw_mutex->readers_active == 0 && rw_mutex->writers_active == 0)
        {
            pthread_cond_signal(&rw_mutex->writers_active_cond); // give priority to writers
        }
    }
    else
    {
        pthread_cond_broadcast(&rw_mutex->readers_active_cond); // wake up all readers that are waiting to read
    }
    pthread_mutex_unlock(&rw_mutex->mutex);
}

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
    }
}

void *client_handler(void *arg)
{
    // this is to used in the pthread_create function to handle each client connection
    int connfd = (int)(long)arg;
    char str_msg[STR_SIZE];

    // Init variables for timing
    double start, end, diff;

    ClientRequest rqst;
    if (read(connfd, str_msg, STR_SIZE) < 0)
    {
        int errsv = errno;
        printf("read() failed with errno = %d. \n", errsv);
        exit(errsv);
    }
    ParseMsg(str_msg, &rqst); // write to the rqst structure
    GET_TIME(start);
    // check for read or write request
    if (rqst.is_read == 1)
    { // read request
        rw_reader_lock(&rw_mutex);
        getContent(rqst.msg, rqst.pos, theArray);
        rw_unlock(&rw_mutex);
        memcpy(str_msg, rqst.msg, STR_SIZE); // send back to client
        if (write(connfd, str_msg, STR_SIZE) < 0)
        {
            int errsv = errno;
            printf("write() failed with errno = %d. \n", errsv);
            exit(errsv);
        }
    }
    else
    { //  write request
        rw_writer_lock(&rw_mutex);
        setContent(rqst.msg, rqst.pos, theArray);
        getContent(rqst.msg, rqst.pos, theArray); // read back the content to confirm
        rw_unlock(&rw_mutex);
        if (write(connfd, str_msg, STR_SIZE) < 0)
        {
            int errsv = errno;
            printf("write() failed with errno = %d. \n", errsv);
            exit(errsv);
        }
    }

    GET_TIME(end);
    diff = end - start;

    pthread_mutex_lock(&latency_mutex.mutex);
    latency_mutex.latencies[latency_mutex.latency_num] = diff;
    latency_mutex.latency_num++;
    if (latency_mutex.latency_num == COM_NUM_REQUEST)
    {
        saveTimes(latency_mutex.latencies, COM_NUM_REQUEST);
        latency_mutex.latency_num = 0;
    }

    pthread_mutex_unlock(&latency_mutex.mutex);

    close(connfd);
    sem_post(&thread_sem);
    pthread_exit(NULL); // end thread and create new one for next client
}

int main(int argc, char *argv[])
{
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

    sem_init(&thread_sem, 0, COM_NUM_REQUEST);

    int sockfd, connfd;
    struct sockaddr_in servaddr;

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

    // CLIENT CONNECTION FAIL -> BRUHHHH
    // servaddr.sin_port = htons(SERVER_PORT);
    servaddr.sin_port = SERVER_PORT;

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

            init_rw_mutex(&rw_mutex);
            init_latency_mutex(&latency_mutex);

            while (1) // loop infinity
            {
                // Mutlithreaded server setup
                connfd = accept(sockfd, NULL, NULL);
                // printf("Connected to client %d\n", connfd);
                sem_wait(&thread_sem);

                pthread_t pthread_handle;
                pthread_create(&pthread_handle, NULL, client_handler, (void *)(long)connfd);
                pthread_detach(pthread_handle);
            }
            close(sockfd);
        }
    }
    else
    {
        printf("socket creation failed\n");
    }

    deallocMem();
}