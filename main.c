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

//DEBUG
pthread_mutex_t peak_mutex = PTHREAD_MUTEX_INITIALIZER;
int active_threads = 0;
int peak_threads = 0;

char **theArray;
int arrayLen;
char *SERVER_IP;
long SERVER_PORT;

typedef struct {
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

typedef struct {
    // pthread_t pthread_handle;
    pthread_mutex_t mutex;
    pthread_cond_t readers_active_cond;
    pthread_cond_t writers_active_cond;
    int readers_active;
    int writers_active;
    int pending_writers;
} rw_mutex_t;

rw_mutex_t rw_mutex;

pthread_t thread_handles[COM_NUM_REQUEST];

void init_rw_mutex(rw_mutex_t *rw_mutex) // initialize the rw_mutex
{
    // //DEBUG
    // printf("Mutex initialized\n");
    rw_mutex -> readers_active = 0;
    rw_mutex -> writers_active = 0;
    rw_mutex -> pending_writers = 0;
    pthread_mutex_init(&rw_mutex->mutex, NULL);
    pthread_cond_init(&rw_mutex->readers_active_cond, NULL);
    pthread_cond_init(&rw_mutex->writers_active_cond, NULL);
}

// create a write lock
void rw_writer_lock(rw_mutex_t *rw_mutex)
{
    // //DEBUG
    // printf("Create write lock\n");
    pthread_mutex_lock(&rw_mutex->mutex);
    rw_mutex->pending_writers++;
    // //DEBUG
    // printf("Pending writers: %d\n", rw_mutex->pending_writers);
    while (rw_mutex->readers_active > 0 || rw_mutex->writers_active > 0)
    {
        // //DEBUG
        // printf("Waiting drives you crazy (writer)\n");
        pthread_cond_wait(&rw_mutex->writers_active_cond, &rw_mutex->mutex); // wait until there is no active reader or writer
    }
    rw_mutex->writers_active++;
    // //DEBUG
    // printf("Active writers: %d\n", rw_mutex->writers_active);
    rw_mutex->pending_writers--;
    pthread_mutex_unlock(&rw_mutex->mutex);
    // //DEBUG
    // printf("Write lock created\n");
}

// create the reader lock
void rw_reader_lock(rw_mutex_t *rw_mutex)
{
    // //DEBUG
    // printf("Create read lock\n");
    pthread_mutex_lock(&rw_mutex->mutex);
    while (rw_mutex->writers_active > 0 || rw_mutex->pending_writers > 0)
    {
        // //DEBUG
        // printf("Waiting drives you crazy (reader)\n");
        pthread_cond_wait(&rw_mutex->readers_active_cond, &rw_mutex->mutex); // wait until there is no active writer or pending writer
    }
    rw_mutex->readers_active++;
    // //DEBUG
    // printf("Active readers: %d\n", rw_mutex->readers_active);
    pthread_mutex_unlock(&rw_mutex->mutex);
}

// unlock function for both reader and writer
void rw_unlock(rw_mutex_t *rw_mutex){
    // //DEBUG
    // printf("Unlocking r/w\n");
    pthread_mutex_lock(&rw_mutex->mutex);
    if (rw_mutex->writers_active > 0)
    {
        rw_mutex->writers_active--;
        // //DEBUG
        // printf("New writers active: %d\n", rw_mutex->writers_active);
    }
    else if (rw_mutex->readers_active > 0)
    {
        rw_mutex->readers_active--;
        // //DEBUG
        // printf("New readers active: %d\n", rw_mutex->readers_active);
    }
    if (rw_mutex->pending_writers > 0)
    {
        // //DEBUG
        // printf("Writers pending: %d\n", rw_mutex->pending_writers);
        if (rw_mutex->readers_active == 0 && rw_mutex->writers_active == 0)
        {
            // //DEBUG
            // printf("No readers or writers active, signal writers_active\n");
            pthread_cond_signal(&rw_mutex->writers_active_cond); // give priority to writers
        }
    }
    else
    {
        // //DEBUG
        // printf("No writers pending, signalling readers_active");
        pthread_cond_broadcast(&rw_mutex->readers_active_cond); // wake up all readers that are waiting to read
    }
    pthread_mutex_unlock(&rw_mutex->mutex);
    // //DEBUG
    // printf("Mutex unlocked\n");
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
        printf("%s\n", theArray[i]);
    }
}

// [TO-DO: Add beef]
void *client_handler(void *arg){
    // this is to used in the pthread_create function to handle each client connection
    int connfd = (int)(long)arg;
    char str_msg[STR_SIZE];
    // rw_mutex_t rw_mutex;

    // Init variables for timing
    double start, end, diff;

    // int str_pos;
    ClientRequest rqst;
    // if(connect(connfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        if (read(connfd, str_msg, STR_SIZE) < 0){
            int errsv = errno;
            printf("read() failed with errno = %d. \n", errsv);
            exit(errsv);
        }
        ParseMsg(str_msg, &rqst); // write to the rqst structure

        //DEBUG
        if(rqst.pos < 0 || rqst.pos >= arrayLen)
        {
            close(connfd);
            pthread_mutex_lock(&peak_mutex);
            active_threads--;
            pthread_mutex_unlock(&peak_mutex);

            pthread_exit(NULL);
        }


        GET_TIME(start);
        // check for read or write request
        if(rqst.is_read == 1){    // read request
            rw_reader_lock(&rw_mutex);
            getContent(rqst.msg, rqst.pos, theArray);
            rw_unlock(&rw_mutex);
            memcpy(str_msg, rqst.msg, STR_SIZE); // send back to client
            if (write(connfd, str_msg, STR_SIZE) < 0){
                int errsv = errno;
                printf("write() failed with errno = %d. \n", errsv);
                exit(errsv);
            }
            // close the connection?
            // close(connfd);
        }
        else{   //  write request
            rw_writer_lock(&rw_mutex);
            setContent(rqst.msg, rqst.pos, theArray);
            getContent(rqst.msg, rqst.pos, theArray); // read back the content to confirm
            rw_unlock(&rw_mutex);
            if (write(connfd, str_msg, STR_SIZE) < 0){
                int errsv = errno;
                printf("write() failed with errno = %d. \n", errsv);
                exit(errsv);
            }
            // close the connection?
            // close(connfd);
        }

        GET_TIME(end);
        diff = end - start;

        pthread_mutex_lock(&latency_mutex.mutex);
        latency_mutex.latencies[latency_mutex.latency_num] = diff;
        latency_mutex.latency_num++;
        if(latency_mutex.latency_num == COM_NUM_REQUEST)
        {
            saveTimes(latency_mutex.latencies, COM_NUM_REQUEST);
            latency_mutex.latency_num = 0;
        }

        pthread_mutex_unlock(&latency_mutex.mutex);

        //DEBUG
        pthread_mutex_lock(&peak_mutex);
        active_threads--;
        pthread_mutex_unlock(&peak_mutex);

    // }
    close(connfd);
    pthread_exit(NULL); // end thread and create new one for next client

}

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
            // pthread_mutex_init(&rw_mutex.mutex, NULL); // initialize the mutex as soon as the server starts working

            init_rw_mutex(&rw_mutex); // initialize the rw_mutex
            init_latency_mutex(&latency_mutex);

            while (1) // loop infinity
            {
                for(int i = 0; i < COM_NUM_REQUEST; ++i)
                {
                    // Mutlithreaded server setup
                    connfd = accept(sockfd, NULL, NULL);

                    pthread_mutex_lock(&peak_mutex);
                    active_threads++;
                    if (active_threads > peak_threads)
                        peak_threads = active_threads;
                    printf("[SERVER] active=%d peak=%d\n",
                        active_threads, peak_threads);
                    pthread_mutex_unlock(&peak_mutex);

                    pthread_create(&thread_handles[i], NULL, client_handler, (void*)(long)connfd);
                }
            }

            for(int i = 0; i < COM_NUM_REQUEST; ++i)
            {
                pthread_join(thread_handles[i], NULL);
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