/*
    Stress client that mimics attacker behavior exactly,
    scaled to ~1000 concurrent reader/writer threads
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <math.h>
#include <string.h>
#include "common.h"

/* ================= CONFIG ================= */

#define TOTAL_THREADS 1000
#define WRITER_RATIO 0.75

int NUM_MSG_ = 4;
int NUM_READER_;
int NUM_WRITER_;
int NUM_THREADS_ = TOTAL_THREADS;
int NUM_ATTACKES_ = 4;

/* ========================================= */

int is_valid_ = 1;
char **msg_pool;
unsigned int *seed_;
struct sockaddr_in sock_var_;
int NUM_STR_;
int attack_pos_;

int PushPullMessage(char *str_msg, char *str_rcv);
void rand_str(char *dest, size_t length);
int is_valid(char *str);

/* ========================================= */

void *Writer(void *rank)
{
    long my_rank = (long)rank;
    char str_msg[COM_BUFF_SIZE], str_rcv[COM_BUFF_SIZE];
    int num_request = COM_NUM_REQUEST / NUM_THREADS_;

    for (int i = 0; i < num_request; ++i)
    {
        int pos = attack_pos_;
        int is_read = 0;
        int msg_idx = rand_r(&seed_[my_rank]) % NUM_MSG_;

        sprintf(str_msg, "%d-%d-%s", pos, is_read, msg_pool[msg_idx]);
        PushPullMessage(str_msg, str_rcv);

        if (COM_IS_VERBOSE)
        {
            printf("Writer %ld sent: %s\n\t recv: %s\n", my_rank, str_msg, str_rcv);
        }
    }
    return NULL;
}

void *Reader(void *rank)
{
    long my_rank = (long)rank;
    char str_msg[COM_BUFF_SIZE], str_rcv[COM_BUFF_SIZE];
    int num_request = COM_NUM_REQUEST / NUM_THREADS_;

    for (int i = 0; i < num_request; ++i)
    {
        int pos = attack_pos_;
        int is_read = 1;
        int msg_idx = rand_r(&seed_[my_rank]) % NUM_MSG_;

        sprintf(str_msg, "%d-%d-%s", pos, is_read, msg_pool[msg_idx]);
        PushPullMessage(str_msg, str_rcv);

        if (i > 1)
        {
            is_valid(str_rcv);
        }

        if (COM_IS_VERBOSE)
        {
            printf("Reader %ld sent: %s\n\t recv: %s\n", my_rank, str_msg, str_rcv);
        }
    }
    return NULL;
}

/* ========================================= */

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <ArraySize> <server ip> <server port>\n", argv[0]);
        exit(1);
    }

    NUM_STR_ = atoi(argv[1]);
    char *server_ip = argv[2];
    int server_port = atoi(argv[3]);

    NUM_WRITER_ = (int)(TOTAL_THREADS * WRITER_RATIO);
    NUM_READER_ = TOTAL_THREADS - NUM_WRITER_;

    /* Message pool */
    msg_pool = malloc(NUM_MSG_ * sizeof(char *));
    for (int i = 0; i < NUM_MSG_; ++i)
    {
        msg_pool[i] = malloc(COM_BUFF_SIZE);
        rand_str(msg_pool[i], COM_BUFF_SIZE - 8);
    }

    /* Seeds */
    seed_ = malloc(NUM_THREADS_ * sizeof(unsigned int));
    for (int i = 0; i < NUM_THREADS_; i++)
        seed_[i] = i + 1;

    pthread_t *threads = malloc(NUM_THREADS_ * sizeof(pthread_t));

    /* Socket info */
    sock_var_.sin_family = AF_INET;
    sock_var_.sin_port = htons(server_port);
    sock_var_.sin_addr.s_addr = inet_addr(server_ip);

    /* Attack loop */
    for (int a = 0; a < NUM_ATTACKES_; ++a)
    {
        attack_pos_ = (NUM_STR_ / NUM_ATTACKES_) * a;

        for (long t = 0; t < NUM_WRITER_; t++)
            pthread_create(&threads[t], NULL, Writer, (void *)t);

        for (long t = NUM_WRITER_; t < NUM_THREADS_; t++)
            pthread_create(&threads[t], NULL, Reader, (void *)t);

        for (int t = 0; t < NUM_THREADS_; t++)
            pthread_join(threads[t], NULL);
    }

    /* Cleanup */
    for (int i = 0; i < NUM_MSG_; ++i)
        free(msg_pool[i]);
    free(msg_pool);
    free(seed_);
    free(threads);

    if (is_valid_)
    {
        printf("Congratulations! Locks held under 1000-thread stress.\n");
        return 0;
    }
    else
    {
        printf("Inconsistency detected under stress.\n");
        return 1;
    }
}

/* ========================================= */

int PushPullMessage(char *str_msg, char *str_rcv)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        exit(errno);

    if (connect(fd, (struct sockaddr *)&sock_var_, sizeof(sock_var_)) < 0)
        exit(errno);

    write(fd, str_msg, COM_BUFF_SIZE);
    read(fd, str_rcv, COM_BUFF_SIZE);

    close(fd);
    return 0;
}

void rand_str(char *dest, size_t length)
{
    char charset[] =
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    while (length--)
        *dest++ = charset[rand() % (sizeof(charset) - 1)];
    *dest = '\0';
}

int is_valid(char *str)
{
    for (int i = 0; i < NUM_MSG_; ++i)
    {
        if (strcmp(str, msg_pool[i]) == 0)
            return 1;
    }
    is_valid_ = 0;
    return 0;
}
