#include <stdio.h>
#include <sys/ipc.h>
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include "ksocket.h"

#define SOCKET_NUMBER 10
#define WINDOW_SIZE 10
#define MESSAGE_SIZE 512
#define KEY_STRING "/"
#define VAL 65

#define MSG_DATA 1
#define MSG_ACK 2

// Message structures with union
struct ack_fields {
    uint8_t last_inorder;
    uint8_t rwnd;
};

struct data_fields {
    char data[510];
};

struct message {
    uint8_t type;
    uint8_t seq_num;
    union {
        struct ack_fields ack;
        struct data_fields data;
    } content;
};

// Global shared memory pointer
socket_shm* shared_memory;
int shmid;

// Function prototypes
void send_ack(int sockfd, const char* dest_ip, int dest_port, uint8_t last_inorder, uint8_t rwnd);
void process_data_message(socket_shm* sock, struct message* msg, struct sockaddr_in* sender);
void process_ack_message(socket_shm* sock, struct message* msg);
void send_message(socket_shm* sock, int slot_index);

void send_ack(int sockfd, const char* dest_ip, int dest_port, uint8_t last_inorder, uint8_t rwnd) {
    struct message msg;
    struct sockaddr_in dest_addr;

    msg.type = MSG_ACK;
    msg.seq_num = 0;  // Not used for ACKs
    msg.content.ack.last_inorder = last_inorder;
    msg.content.ack.rwnd = rwnd;

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dest_port);
    dest_addr.sin_addr.s_addr = inet_addr(dest_ip);

    sendto(sockfd, &msg, sizeof(struct message), 0, 
           (struct sockaddr*)&dest_addr, sizeof(dest_addr));
}

void process_data_message(socket_shm* sock, struct message* msg, struct sockaddr_in* sender) {
    pthread_mutex_lock(&sock->lock);

    if (msg->seq_num == sock->exp_seq_num) {
        // In-order message
        if (sock->receive_buffer_count < WINDOW_SIZE) {
            memcpy(sock->receive_buffer[sock->receive_buffer_count], 
                   msg->content.data.data, 
                   sizeof(msg->content.data.data));
            sock->receive_buffer_count++;
            sock->exp_seq_num++;

            // Send ACK
            send_ack(sock->udp_sockfd, 
                    sock->peer_ip, 
                    sock->peer_port,
                    sock->exp_seq_num - 1,
                    WINDOW_SIZE - sock->receive_buffer_count);
        } else {
            sock->nospace = true;
        }
    }

    pthread_mutex_unlock(&sock->lock);
}

void process_ack_message(socket_shm* sock, struct message* msg) {
    pthread_mutex_lock(&sock->lock);

    int acked_seq = msg->content.ack.last_inorder;
    
    // Update window size based on receiver's advertised window
    sock->swnd.window_size = msg->content.ack.rwnd;

    // Mark messages as acknowledged and slide window
    for (int i = 0; i < WINDOW_SIZE; i++) {
        if (sock->swnd.slots[i].seq_num <= acked_seq && 
            sock->swnd.slots[i].is_sent &&
            !sock->swnd.slots[i].is_acked) {
            
            sock->swnd.slots[i].is_acked = true;
            
            // If this is the base, slide window
            if (sock->swnd.slots[i].seq_num == sock->swnd.base) {
                while (i < WINDOW_SIZE && sock->swnd.slots[i].is_acked) {
                    i++;
                }
                sock->swnd.base = (i < WINDOW_SIZE) ? 
                                  sock->swnd.slots[i].seq_num : 
                                  sock->swnd.next_seq_num;
            }
        }
    }

    pthread_mutex_unlock(&sock->lock);
}

void send_message(socket_shm* sock, int slot_index) {
    struct message msg;
    struct sockaddr_in dest_addr;

    msg.type = MSG_DATA;
    msg.seq_num = sock->swnd.slots[slot_index].seq_num;
    memcpy(msg.content.data.data, 
           sock->swnd.slots[slot_index].data,
           sizeof(msg.content.data.data));

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(sock->peer_port);
    dest_addr.sin_addr.s_addr = inet_addr(sock->peer_ip);

    sendto(sock->udp_sockfd, &msg, sizeof(struct message), 0,
           (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    sock->swnd.slots[slot_index].is_sent = true;
    sock->swnd.slots[slot_index].send_time = time(NULL);
}

void* receiver_thread(void* arg) {
    fd_set readfds;
    struct timeval tv;
    
    while (1) {
        FD_ZERO(&readfds);
        int max_fd = 0;

        // Add all active sockets to fd_set
        for (int i = 0; i < SOCKET_NUMBER; i++) {
            if (shared_memory[i].isAlloted) {
                FD_SET(shared_memory[i].udp_sockfd, &readfds);
                max_fd = (shared_memory[i].udp_sockfd > max_fd) ? 
                         shared_memory[i].udp_sockfd : max_fd;
            }
        }

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ready = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ready < 0) continue;

        for (int i = 0; i < SOCKET_NUMBER; i++) {
            if (!shared_memory[i].isAlloted) continue;

            if (FD_ISSET(shared_memory[i].udp_sockfd, &readfds)) {
                struct message msg;
                struct sockaddr_in sender_addr;
                socklen_t addr_len = sizeof(sender_addr);

                ssize_t received = recvfrom(shared_memory[i].udp_sockfd, 
                                          &msg, sizeof(msg), 0,
                                          (struct sockaddr*)&sender_addr, 
                                          &addr_len);

                if (received > 0 && !dropMessage(P)) {
                    if (msg.type == MSG_DATA) {
                        process_data_message(&shared_memory[i], &msg, &sender_addr);
                    } else if (msg.type == MSG_ACK) {
                        process_ack_message(&shared_memory[i], &msg);
                    }
                }
            }

            // Handle nospace condition
            pthread_mutex_lock(&shared_memory[i].lock);
            if (shared_memory[i].nospace && 
                shared_memory[i].receive_buffer_count < WINDOW_SIZE) {
                send_ack(shared_memory[i].udp_sockfd,
                        shared_memory[i].peer_ip,
                        shared_memory[i].peer_port,
                        shared_memory[i].exp_seq_num - 1,
                        WINDOW_SIZE - shared_memory[i].receive_buffer_count);
                shared_memory[i].nospace = false;
            }
            pthread_mutex_unlock(&shared_memory[i].lock);
        }
    }
    return NULL;
}

void* sender_thread(void* arg) {
    while (1) {
        sleep(T/2);

        for (int i = 0; i < SOCKET_NUMBER; i++) {
            if (!shared_memory[i].isAlloted) continue;

            pthread_mutex_lock(&shared_memory[i].lock);

            time_t current_time = time(NULL);

            // Check for timeouts and retransmit
            for(int j = 0; j < WINDOW_SIZE; j++) {
                if (shared_memory[i].swnd.slots[j].is_sent && !shared_memory[i].swnd.slots[j].is_acked 
                    && (current_time - shared_memory[i].swnd.slots[j].send_time >= T)) 
                {
                    send_message(&shared_memory[i], j);
                }
            }

            // Send new messages if possible
            while(((shared_memory[i].swnd.next_seq_num - shared_memory[i].swnd.base) < shared_memory[i].swnd.window_size) 
                    && shared_memory[i].send_buffer_count > 0) {
                
                // Find empty slot
                for(int j = 0; j < WINDOW_SIZE; j++) {
                    if(!shared_memory[i].swnd.slots[j].is_sent || shared_memory[i].swnd.slots[j].is_acked) {
                        
                        // Prepare slot
                        shared_memory[i].swnd.slots[j].seq_num  = shared_memory[i].swnd.next_seq_num++;
                        shared_memory[i].swnd.slots[j].is_sent  = false;
                        shared_memory[i].swnd.slots[j].is_acked = false;
                        
                        // Copy data from send buffer
                        memcpy(shared_memory[i].swnd.slots[j].data, shared_memory[i].send_buffer[0], MESSAGE_SIZE);

                        // Shift send buffer
                        for (int k = 0; k < shared_memory[i].send_buffer_count - 1; k++) {
                            memcpy(shared_memory[i].send_buffer[k], shared_memory[i].send_buffer[k + 1], MESSAGE_SIZE);
                        }
                        shared_memory[i].send_buffer_count--;

                        // Send the message
                        send_message(&shared_memory[i], j);
                        break;
                    }
                }
            }

            pthread_mutex_unlock(&shared_memory[i].lock);
        }
    }
    return NULL;
}

void* garbage_collector(void* arg) {
    while (1) {
        sleep(1);
        for (int i = 0; i < SOCKET_NUMBER; i++) {
            if (shared_memory[i].isAlloted) {
                if (kill(shared_memory[i].pid, 0) < 0) {
                    pthread_mutex_lock(&shared_memory[i].lock);
                    shared_memory[i].isAlloted = false;
                    shared_memory[i].pid = 0;
                    pthread_mutex_unlock(&shared_memory[i].lock);
                }
            }
        }
    }
    return NULL;
}

int main() {
    // Initialize random seed for dropMessage
    srand(time(NULL));

    // Create shared memory
    key_t key = ftok(KEY_STRING, VAL);
    shmid = shmget(key, SOCKET_NUMBER * sizeof(socket_shm), 0777 | IPC_CREAT);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    shared_memory = (socket_shm*)shmat(shmid, NULL, 0);
    if (shared_memory == (void*)-1) {
        perror("shmat");
        exit(1);
    }

    // Initialize shared memory
    for (int i = 0; i < SOCKET_NUMBER; i++) {
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&shared_memory[i].lock, &attr);

        shared_memory[i].isAlloted = false;
        shared_memory[i].swnd.window_size = WINDOW_SIZE;
        shared_memory[i].rwnd.window_size = WINDOW_SIZE;
        shared_memory[i].swnd.base = 1;
        shared_memory[i].swnd.next_seq_num = 1;
        shared_memory[i].exp_seq_num = 1;
        shared_memory[i].send_buffer_count = 0;
        shared_memory[i].receive_buffer_count = 0;
        shared_memory[i].nospace = false;
    }

    // Create threads
    pthread_t receiver, sender, gc;
    pthread_create(&receiver, NULL, receiver_thread, NULL);
    pthread_create(&sender, NULL, sender_thread, NULL);
    pthread_create(&gc, NULL, garbage_collector, NULL);

    // Wait for threads
    pthread_join(receiver, NULL);
    pthread_join(sender, NULL);
    pthread_join(gc, NULL);

    // Cleanup
    shmdt(shared_memory);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}