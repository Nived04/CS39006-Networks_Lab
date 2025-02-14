#ifndef KSOCKET_H
#define KSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#define SOCK_KTP 100
#define T 5  // Timeout value in seconds
#define P 0.1 // Default drop probability

// Error codes
#define ENOSPACE 1    // No space available in socket array
#define ENOTBOUND 2   // Destination address doesn't match bound address
#define ENOMESSAGE 3  // No message available in receive buffer

#define MESSAGE_SIZE 512
#define WINDOW_SIZE 10

typedef struct {
    int start_seq_no; // starting sequence number of the window
    int swnd_size; // size of sending window (must be < available buffer space at receiver)
    int seq_no_not_acked[10]; // list of sequence numbers sent but not acked
}sending_window;

typedef struct {    
    int rwnd_size; // size of receiving window depending on available buffer space
    int expected_seq_no[10]; // sequence numbers expected to be received
}receiving_window;

// Improved window structure
typedef struct {
    int base;              // Start of the window
    int next_seq_num;      // Next sequence number to be used
    int window_size;       // Current size of the window
    struct {
        uint8_t seq_num;   // Sequence number of this slot
        bool is_sent;      // Has this been sent?
        bool is_acked;     // Has this been acknowledged?
        time_t send_time;  // When was it last sent
        char data[MESSAGE_SIZE]; // The actual message data
    } slots[WINDOW_SIZE];
} window;

// Socket structure
typedef struct {
    pthread_mutex_t lock;
    bool isAlloted;
    int pid;
    int udp_sockfd;
    int peer_port;
    char peer_ip[16];
    char send_buffer[10][MESSAGE_SIZE];
    char receive_buffer[10][MESSAGE_SIZE];
    int send_buffer_count;
    int receive_buffer_count;
    window swnd;           // Sending window
    window rwnd;           // Receiving window
    uint8_t exp_seq_num;   // Next expected sequence number
    bool nospace;          // Flag for buffer full condition
} socket_shm;

// Global error variable
extern int error_var;

// Function declarations
int k_socket(int domain, int type, int protocol);
int k_bind(int sockfd, char* source_ip, int source_port, char* dest_ip, int dest_port);
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
int k_close(int sockfd);

// Message drop simulation
int dropMessage(float p);

#endif