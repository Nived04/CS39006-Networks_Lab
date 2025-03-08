#include <stdbool.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <sys/shm.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

#define ENOMESSAGE  1111
#define ENOSPACE   1112
#define ENOTBOUND   1113
#define KEY_STRING "/home/nived/test"
#define VAL 65

#define SOCK_KTP 100

#define MAX_MESSAGE_SIZE 512
#define MAX_WINDOW_SIZE 10

#define MAX_SOCKETS 10
#define MAX_BUFFER_SIZE 10
#define MAX_SEQ_NUM 255

#define T 10
#define P 0.0

typedef struct {
    char data[MAX_MESSAGE_SIZE - 8];
}data_message;

typedef struct {
    int expected_seq_num;
    int rwnd;
}ack_message;

typedef struct {
    int type;
    int seq_num;
    union {
        data_message data;
        ack_message ack;
    }content;
}message;

typedef struct { 
    int next_seq_num;
    bool slot_empty[MAX_BUFFER_SIZE];
    message buffer[MAX_BUFFER_SIZE];
    time_t timeout[MAX_WINDOW_SIZE];
}sending_buffer;

typedef struct {
    int last_seq;
    bool received[MAX_WINDOW_SIZE];
    message buffer[MAX_BUFFER_SIZE];
}receiving_buffer;

typedef struct {
    int base;
    int window_size;
    int last_ack;
    int sequence[MAX_BUFFER_SIZE];
    union {
        receiving_buffer rcv;
        sending_buffer snd;
    }buff;
}buffer;

typedef struct {
    pthread_mutex_t lock;
    bool isAlloted;
    bool isBound;
    bool nospace;
    bool isClosed;
    int sockfd;
    int pid;
    time_t RTO;
    // time_t last_sent_time;
    struct sockaddr_in peer_addr; 
    struct sockaddr_in self_addr;
    buffer s_buff;
    buffer r_buff;
}ktp_socket;

ktp_socket* attach_ktp_socket();

int get_socket_num(int, ktp_socket*, int);
int k_socket(int, int, int);
int k_bind(int, const char*, int, const char*, int);
ssize_t k_sendto(int, const void*, size_t, int, const struct sockaddr*, socklen_t);
ssize_t k_recvfrom(int, void*, size_t, int, struct sockaddr*, socklen_t*);
int k_close(int);

void init_sending_buffer(buffer*);
void init_receiving_buffer(buffer*);

bool dropMessage(float p);