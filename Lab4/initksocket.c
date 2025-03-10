/*
Assignment 4 Submission
Name: Nived Roshan Shah
RollNo: 22CS10049
*/

#include "ksocket.h"

fd_set master;

void init_socket_sm() {
    key_t key = ftok(KEY_STRING, VAL);
    int shmid = shmget(key, MAX_SOCKETS * sizeof(ktp_socket), 0777 | IPC_CREAT);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    ktp_socket* M = (ktp_socket*)shmat(shmid, NULL, 0);
    
    // Initialize mutex attributes for cross-process sharing
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    
    for (int i = 0; i < MAX_SOCKETS; i++) {
        M[i].isAlloted = false;
        pthread_mutex_init(&M[i].lock, &attr);
    }
    
    pthread_mutexattr_destroy(&attr);

    printf("Socket shared memory initialized with id: %d\n", shmid);
    shmdt(M);
}

int send_ack(int sockfd, struct sockaddr_in dest_addr, int seq, int rwnd){
    int type = 0;
    
    message msg;
    msg.type = type;
    msg.seq_num = seq;
    msg.content.ack.rwnd = rwnd;

    printf("\nS: Sending ACK for seq no: %d, with window_size: %d\n", seq, rwnd);

    return sendto(sockfd, &msg, MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

void cleanup(int sig) {
    key_t key = ftok(KEY_STRING, VAL);
    int shmid = shmget(key, 0, 0);
    ktp_socket* M = (ktp_socket*)shmat(shmid, NULL, 0);
    
    for(int i=0; i<MAX_SOCKETS; i++)
        pthread_mutex_destroy(&M[i].lock);

    if(shmid != -1) {
        shmctl(shmid, IPC_RMID, 0);
        printf("Shared memory %d removed\n", shmid);
    }

    if(sig == SIGSEGV) {
        printf("Segmentation fault\n");
    }

    exit(0);
}

void* receiver_thread(void* arg) {
    printf("In Receiver thread\n");
    fd_set readfds;
    int maxfd = 0;
    struct timeval tv;

    ktp_socket* SM = attach_ktp_socket();

    message msg;

    while(1) {
        readfds = master;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        
        select(maxfd + 1, &readfds, NULL, NULL, &tv);
        
        int recvsocket = -1;
        int sock_index = -1;
        ssize_t numbytes = -1;
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);

        // check if a file descriptor is ready and receive message
        for(int i=0; i<MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].lock);
            if(SM[i].isAlloted && SM[i].isBound && FD_ISSET(SM[i].sockfd, &readfds)) {
                recvsocket = SM[i].sockfd;
                sock_index = i;
                numbytes = recvfrom(recvsocket, &msg, MAX_MESSAGE_SIZE, 0, (struct sockaddr*)&sender_addr, &addr_len);
                
                // printf("\nR: Received message on socket: %d\n", i);

                if(numbytes < 0) {
                    printf("\nR: Error in recvfrom\n");
                } 
                else if(numbytes == 0){
                    printf("\nR: Connection closed by client\n");
                    close(recvsocket);
                    SM[i].isAlloted = false;
                    FD_CLR(recvsocket, &master);
                }
            }

            pthread_mutex_unlock(&SM[i].lock);

            if(recvsocket != -1) break;
        }

        // a message was received on sockfd = recvsocket
        if(recvsocket != -1) {
            pthread_mutex_lock(&SM[sock_index].lock);

            if(SM[sock_index].isAlloted 
                && SM[sock_index].peer_addr.sin_addr.s_addr == sender_addr.sin_addr.s_addr 
                && SM[sock_index].peer_addr.sin_port == sender_addr.sin_port) 
            {
                int type, seq;
                
                type = msg.type;
                seq = msg.seq_num;

                if(dropMessage(P)) {
                    printf("\nR: Dropped message of seq no: %d, from socket: %d\n", seq, sock_index);
                    pthread_mutex_unlock(&SM[sock_index].lock);
                    continue;
                }

                // if it is a DATA message:
                if(type == 1) {
                    printf("\nR: Received DATA message on socket: %d, with seq no: %d\n", sock_index, seq);

                    SM[sock_index].nospace = false;
                    bool duplicate = true;
                    
                    int j = SM[sock_index].r_buff.base;

                    printf("\nR: Current base and its expected sequence number: %d, %d\n", SM[sock_index].r_buff.base, SM[sock_index].r_buff.sequence[SM[sock_index].r_buff.base]);
                        
                    for(int x = 0; x < SM[sock_index].r_buff.window_size; x++, j=(j+1)%MAX_WINDOW_SIZE) {
                        if(SM[sock_index].r_buff.sequence[j] == seq) { 
                            if(!SM[sock_index].r_buff.buff.rcv.received[j]) { 

                                duplicate = false;
                                SM[sock_index].r_buff.buff.rcv.received[j] = true;
                                memcpy(&SM[sock_index].r_buff.buff.rcv.buffer[j], &msg, MAX_MESSAGE_SIZE);

                                int new_last_ack_slot = -1;
                                
                                int k = SM[sock_index].r_buff.base;
                                for (int ct = 0; ct < SM[sock_index].r_buff.window_size; ct++, k=(k+1)%MAX_WINDOW_SIZE) {
                                    if (!SM[sock_index].r_buff.buff.rcv.received[k])
                                        break;
                                    new_last_ack_slot = k;
                                }

                                if(new_last_ack_slot != -1) {
                                    SM[sock_index].r_buff.last_ack = SM[sock_index].r_buff.sequence[new_last_ack_slot];
                                    
                                    k = SM[sock_index].r_buff.base;
                                    while(true) {
                                        SM[sock_index].r_buff.sequence[k] = (SM[sock_index].r_buff.buff.rcv.last_seq)%MAX_SEQ_NUM + 1;
                                        SM[sock_index].r_buff.buff.rcv.last_seq = SM[sock_index].r_buff.sequence[k];

                                        if(k == new_last_ack_slot) {
                                            break;
                                        }

                                        k = (k+1)%MAX_WINDOW_SIZE;
                                    }

                                    SM[sock_index].r_buff.window_size -= (new_last_ack_slot - SM[sock_index].r_buff.base + MAX_WINDOW_SIZE)%MAX_WINDOW_SIZE + 1;
                                    SM[sock_index].r_buff.base = (new_last_ack_slot + 1)%MAX_WINDOW_SIZE;

                                    int n = send_ack(SM[sock_index].sockfd, SM[sock_index].peer_addr, SM[sock_index].r_buff.last_ack, SM[sock_index].r_buff.window_size);
                                    if(n < 0) printf("\nR: Error in sending ACK\n");
                                    // else 
                                        // printf("\nR: Sent ACK for seq no: %d from socket: %d\n", SM[i].r_buff.last_ack, i);
                                }
                            }
                            break;
                        }
                    }

                    if(duplicate) {
                        printf("\nR: Duplicate message: %d\n", seq);
                        int n = send_ack(SM[sock_index].sockfd, SM[sock_index].peer_addr, SM[sock_index].r_buff.last_ack, SM[sock_index].r_buff.window_size);
                        if(n < 0) printf("\nR: Error in sending ACK\n");
                    }

                    if(SM[sock_index].r_buff.window_size == 0) SM[sock_index].nospace = true;
                }
                // if message is ACK:
                else if(type == 0) {    
                    // printf("\nS: Received ACK for seq no: %d\n", seq);
                    int rwnd = ntohs(msg.content.ack.rwnd);

                    int j = SM[sock_index].s_buff.base;
                    for(int x = 0; x < SM[sock_index].s_buff.window_size; x++, j=(j+1)%MAX_WINDOW_SIZE) {
                        if(SM[sock_index].s_buff.sequence[j] == seq) {
                            int k = SM[sock_index].s_buff.base;
                            while(true) {
                                SM[sock_index].s_buff.buff.snd.timeout[k] = -1;
                                SM[sock_index].s_buff.buff.snd.slot_empty[k] = true; 
                                SM[sock_index].s_buff.sequence[k] = (SM[sock_index].s_buff.buff.snd.last_seq)%MAX_SEQ_NUM + 1;
                                SM[sock_index].s_buff.buff.snd.last_seq = SM[sock_index].s_buff.sequence[k];
                                
                                // printf("\nS: setting next_seq_num to: %d\n", SM[sock_index].s_buff.buff.snd.next_seq_num);

                                if(k == j) break;

                                k = (k+1)%MAX_WINDOW_SIZE;
                            }

                            SM[sock_index].s_buff.base = (j+1)%MAX_WINDOW_SIZE;
                            break;
                        }
                    }
                    
                    SM[sock_index].s_buff.window_size = rwnd;
                }
            }

            pthread_mutex_unlock(&SM[sock_index].lock);
        }
        // no file descriptor is ready
        else {
            for(int i=0; i<MAX_SOCKETS; i++) {
                pthread_mutex_lock(&SM[i].lock);

                if(SM[i].isAlloted == true) {
                    if(!SM[i].isBound) {

                        if(SM[i].sockfd > 0) close(SM[i].sockfd);

                        int ksockfd = socket(AF_INET, SOCK_DGRAM, 0);

                        if(ksockfd < 0) {
                            printf("\nR: Error in socket creation\n");
                        }
                        else {
                            if(bind(ksockfd, (struct sockaddr* )&SM[i].self_addr, sizeof(SM[i].self_addr)) < 0) {
                                printf("\nPort number: %d\n", ntohs(SM[i].self_addr.sin_port));
                                perror("\nR: Error in binding");
                            }
                            else {
                                SM[i].sockfd = ksockfd;
                                SM[i].isBound = true;
                                FD_SET(SM[i].sockfd, &master);
                                if(SM[i].sockfd > maxfd) {
                                    maxfd = SM[i].sockfd;
                                }
                            }
                        }
                    }
                    else if(SM[i].nospace && SM[i].r_buff.window_size > 0) {
                        int n = send_ack(SM[i].sockfd, SM[i].peer_addr, SM[i].r_buff.last_ack, SM[i].r_buff.window_size);
                        if(n < 0) printf("\nR: Error in sending ACK\n");
                    }
                }

                pthread_mutex_unlock(&SM[i].lock);
            }
        }
    }
}

void* sender_thread(void* arg) {
    printf("S: In sender thread\n");
    ktp_socket* SM = attach_ktp_socket();

    while(1) {
        sleep(T/2);

        for(int i=0; i<MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].lock);

            if(SM[i].isAlloted && SM[i].isBound) {
                time_t base_timeout = SM[i].s_buff.buff.snd.timeout[SM[i].s_buff.base];
                time_t time_diff = time(NULL) - base_timeout;
                
                // if timeout has occurred
                if(time_diff >= T && base_timeout > 0) {
                    int j = SM[i].s_buff.base;
                    for(int x = 0; x < SM[i].s_buff.window_size; x++, j=(j+1)%MAX_WINDOW_SIZE) {
                        if(SM[i].s_buff.buff.snd.slot_empty[j] || SM[i].s_buff.buff.snd.timeout[j] < 0) {
                            break;
                        }
                        
                        int n = sendto(SM[i].sockfd, &SM[i].s_buff.buff.snd.buffer[j], MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&SM[i].peer_addr, sizeof(SM[i].peer_addr));
                    
                        if(n < 0) { 
                            printf("\nS: couldn't send message\n"); 
                        }
                    
                        SM[i].s_buff.buff.snd.timeout[j] = time(NULL) + T;
                        
                    }
                }
            }

            pthread_mutex_unlock(&SM[i].lock);
        }

        for(int i=0; i<MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].lock);

            if(SM[i].isAlloted && SM[i].isBound) {
                int j = SM[i].s_buff.base;
                for(int x = 0; x < SM[i].s_buff.window_size; x++, j=(j+1)%MAX_WINDOW_SIZE) {
                    if(SM[i].s_buff.buff.snd.timeout[j] == -1) {
                        if(!SM[i].s_buff.buff.snd.slot_empty[j]) {
                            printf("\nS: Sending message of sequence no: %d through ksocket: %d\n", SM[i].s_buff.buff.snd.buffer[j].seq_num, i);

                            int n = sendto(SM[i].sockfd, &SM[i].s_buff.buff.snd.buffer[j], MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&SM[i].peer_addr, sizeof(SM[i].peer_addr));
                            if(n < 0) { printf("\nS: couldn't send message\n"); } 

                            SM[i].s_buff.buff.snd.timeout[j] = time(NULL) + T;
                        }
                    }
                }
            }

            pthread_mutex_unlock(&SM[i].lock);
        }
    }
}

void* garbage_collector_thread(void* arg) {
    ktp_socket* SM = attach_ktp_socket();

    while(1) {
        sleep(T);

        for(int i=0; i<MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].lock);

            if(SM[i].isAlloted) {
                if(kill(SM[i].pid, 0) == -1) {
                    printf("\nG: Process %d terminated\n", SM[i].pid);
                    SM[i].isAlloted = false;
                    FD_CLR(SM[i].sockfd, &master);
                    close(SM[i].sockfd);
                }
            }

            pthread_mutex_unlock(&SM[i].lock);
        }
    }
}

int main() {
    srand(time(NULL));
    
    init_socket_sm();

    FD_ZERO(&master);

    signal(SIGINT, cleanup);
    signal(SIGSEGV, cleanup);

    pthread_t receiver, sender, garbage_collector;
    pthread_create(&receiver, NULL, receiver_thread, NULL);
    pthread_create(&sender, NULL, sender_thread, NULL);
    pthread_create(&garbage_collector, NULL, garbage_collector_thread, NULL);

    pthread_exit(NULL);
    
} 