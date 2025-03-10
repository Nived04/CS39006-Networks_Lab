/*
Assignment 4 Submission
Name: Nived Roshan Shah
RollNo: 22CS10049
*/

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

// Custom error codes
#define ENOMESSAGE  1111
#define ENOSPACE    1112
#define ENOTBOUND   1113

// Key string and value used for shared memory
#define KEY_STRING  "/"
#define VAL         65

// Custom socket type
#define SOCK_KTP    100

// Constants
#define MAX_SOCKETS      10
#define MAX_SEQ_NUM      256
#define MAX_WINDOW_SIZE  10
#define MAX_BUFFER_SIZE  10
#define MAX_MESSAGE_SIZE 512
#define T 5
#define P 0.05

// struct to represent a DATA-message
typedef struct {
    char data[MAX_MESSAGE_SIZE - 8];
}data_message;

// struct to represent an ACK-message
typedef struct {
    int expected_seq_num;
    int rwnd;
}ack_message;

// struct to represent a message
typedef struct {
    int type;       // 0 for ACK, 1 for DATA
    int seq_num;    // sequence number
    union {
        data_message data;
        ack_message ack;
    }content;
}message;

// struct to represent the sending buffer
typedef struct { 
    int last_seq;
    bool slot_empty[MAX_BUFFER_SIZE];
    message buffer[MAX_BUFFER_SIZE];
    time_t timeout[MAX_WINDOW_SIZE];
}sending_buffer;

// struct to represent the receiving buffer
typedef struct {
    int last_seq;
    bool received[MAX_WINDOW_SIZE];
    message buffer[MAX_BUFFER_SIZE];
}receiving_buffer;

// struct to represent the buffer
typedef struct {
    int base;           // base of the window   
    int window_size;    // window size
    int last_ack;       // last acknowledged sequence number
    int sequence[MAX_BUFFER_SIZE];  // sequence numbers (expected or sent) for the window
    union {
        receiving_buffer rcv;
        sending_buffer snd;
    }buff;              // buffer of type either sending or receiving
}buffer;

// struct to represent the k_socket
typedef struct {
    pthread_mutex_t lock;   // lock for the socket
    bool isAlloted;         // flag to indicate if the socket is alloted
    bool isBound;           // flag to indicate if the socket is bound
    bool nospace;           // flag to indicate if there is no space for new sockets
    int sockfd;             // socket file descriptor of the underlying UDP socket
    int pid;                // process id of the process that created the socket
    struct sockaddr_in peer_addr;   // peer address (destination)
    struct sockaddr_in self_addr;   // self address (source)
    buffer s_buff;          // sending buffer
    buffer r_buff;          // receiving buffer
}ktp_socket;

/*
Attach the shared-memory used for the ksockets. 
It uses the key string and value that is defined in the header to provide the same
key for all the entities wishing to attach the memory
*/
ktp_socket* attach_ktp_socket();

/*
Function that finds the index of the next free (unalloted) socket in the shared memory
*/
int get_socket_num(ktp_socket*);

/*
Function to initialize the sending buffer.
The application (user codes) call k_sendto to send a message (which is stored in the buffer).
The sending thread accesses the buffer periodically to see if any new message is available. 
If so, it sends the message to the destination.
If not, it sees whether a timeout has occured, and sends the message if it has.
*/
void init_sending_buffer(buffer*);

/*
Function to initialize the receiving buffer.
The buffer stores any incoming message from the UDP socket into the receiver buffer. 
The application (user codes) call k_recvfrom to get the message from the buffer if available.
Various checks and algorithms are implemented to ensure that the message is received in order.
*/
void init_receiving_buffer(buffer*);

/*
Function that simulates the dropping of messages.
*/
bool dropMessage(float p);

/*
Function to allocate a k_socket. If allocated, it initializes the socket structure.
*/
int k_socket(int, int, int);

/*
Function that sets the source and destination parameters for the k_socket.
*/
int k_bind(int, const char*, int, const char*, int);

/*
Function which receives the data to be sent as message from the application,
creates a packet (with the message headers) and stores it in the sending buffer.
*/
ssize_t k_sendto(int, const void*, size_t, int, const struct sockaddr*, socklen_t);

/*
Function which copies the message received in the shared receiving buffer to the application's buf 
*/
ssize_t k_recvfrom(int, void*, size_t, int, struct sockaddr*, socklen_t*);

/*
Function that changes the isAlloted flag to false, indicating that the socket is closed,
and a new socket can be allocated in its place.
*/
int k_close(int);