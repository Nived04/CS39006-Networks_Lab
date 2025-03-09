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
    
    // Clean up attributes
    pthread_mutexattr_destroy(&attr);

    printf("Socket shared memory initialized with id: %d\n", shmid);
    shmdt(M);
}

int send_ack(int sockfd, struct sockaddr_in dest_addr, int seq, int rwnd){
    int type = 0;
    // int nseq = htons(seq), nrwnd = htons(rwnd);
    
    // printf("\n*** ACK for Seq No: %d\n", seq);

    message msg;
    msg.type = type;
    msg.seq_num = seq;
    msg.content.ack.rwnd = rwnd;

    return sendto(sockfd, &msg, MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
}

void cleanup(int sig) {
    key_t key = ftok(KEY_STRING, VAL);
    int shmid = shmget(key, 0, 0);

    ktp_socket* M = (ktp_socket*)shmat(shmid, NULL, 0);
    
    for(int i=0; i<MAX_SOCKETS; i++) {
        pthread_mutex_destroy(&M[i].lock);
    }

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

    // FD_ZERO(&master);

    ktp_socket* SM = attach_ktp_socket();

    message msg;

    while(1) {
        readfds = master;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        
        select(maxfd + 1, &readfds, NULL, NULL, &tv);
        
        int recvsocket = -1;
        ssize_t numbytes = -1;
        struct sockaddr_in sender_addr;
        socklen_t addr_len = sizeof(sender_addr);

        for(int i=0; i<MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].lock);
            // printf("TEST\n");
            if(SM[i].isAlloted && SM[i].isBound && FD_ISSET(SM[i].sockfd, &readfds)) {
                recvsocket = SM[i].sockfd;
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
                else {

                    // printf("\nR: Received data of size %zd bytes\n", numbytes);
                    // printf("\nR: Received a message of type = %d, seq_num = %d: \n", msg.type, msg.seq_num);
                    // if(msg.type == 1) {    
                    //     for(int i=0; i<504; i++) {
                    //         printf("%c", msg.content.data.data[i]);
                    //     }
                    //     printf("\n\n");
                    // }

                }
            }

            pthread_mutex_unlock(&SM[i].lock);

            if(recvsocket != -1) {
                break;
            }
        }

        if(recvsocket != -1) {
            for(int i=0; i<MAX_SOCKETS; i++) {
                pthread_mutex_lock(&SM[i].lock);

                if(SM[i].isAlloted && SM[i].sockfd == recvsocket 
                    && SM[i].peer_addr.sin_addr.s_addr == sender_addr.sin_addr.s_addr 
                    && SM[i].peer_addr.sin_port == sender_addr.sin_port) 
                {
                    int type, seq;
                    
                    type = msg.type;
                    seq = msg.seq_num;

                    if(dropMessage(P)) {
                        printf("\nR: Dropped message of seq no: %d, from socket: %d\n", seq, i);
                        pthread_mutex_unlock(&SM[i].lock);
                        continue;
                    }

                    // if it is a DATA message:
                    if(type == 1) {

                        printf("\nR: Received DATA message on socket: %d, with seq no: %d\n", i, seq);
                        /*
                        * in-order message
                            the message is written to the buffer after removing the KTP header, the free space in the buffer is computed and the rwnd size is updated accordingly.
                            The receiver then sends an ACK message to the sender which piggybacks the updated rwnd size, and the sequence number of the last in-order message received within rwnd.
                        * out-of-order message
                            keeps the message in the buffer (if the message sequence number is within rwnd) but does not send any ACK message.
                        * duplicate messages
                            identifying them with the sequence number and then dropping them if already received once
                        */

                        SM[i].nospace = false;
                        bool duplicate = true;
                        
                        int j = SM[i].r_buff.base;

                        // printf("\nR: Current base and its expected sequence number: %d, %d\n", SM[i].r_buff.base, SM[i].r_buff.sequence[SM[i].r_buff.base]);
                        
                        for(int x = 0; x < SM[i].r_buff.window_size; x++, j=(j+1)%MAX_WINDOW_SIZE) {
                            // if sequence matches expected number in the window and the slot has no received messages
                            // then write the message to the buffer and update the last_ack and rwnd
                            if(SM[i].r_buff.sequence[j] == seq) { 
                                if(!SM[i].r_buff.buff.rcv.received[j]) {       
                                    duplicate = false;
                                    SM[i].r_buff.buff.rcv.received[j] = true;
                                    memcpy(&SM[i].r_buff.buff.rcv.buffer[j], &msg, MAX_MESSAGE_SIZE);
                                    
                                    printf("\nRECEIVER:\n---------\n");
                                    fwrite(&SM[i].r_buff.buff.rcv.buffer[j].content.data.data, 1, MAX_MESSAGE_SIZE-8, stdout);
                                    printf("\n-----------\n");

                                    int new_last_ack_slot = -1;
                                    
                                    int k = SM[i].r_buff.base;
                                    for (int ct = 0; ct < SM[i].r_buff.window_size; ct++, k=(k+1)%MAX_WINDOW_SIZE) {
                                        if (!SM[i].r_buff.buff.rcv.received[k])
                                            break;
                                        new_last_ack_slot = k;
                                    }

                                    // printf("\n\n\nR: New last ack slot: %d\n\n", new_last_ack_slot);

                                    if(new_last_ack_slot != -1) {
                                        SM[i].r_buff.last_ack = SM[i].r_buff.sequence[new_last_ack_slot];
                                        
                                        k = SM[i].r_buff.base;
                                        while(true) {
                                            SM[i].r_buff.sequence[k] = (SM[i].r_buff.buff.rcv.last_seq)%MAX_SEQ_NUM + 1;
                                            SM[i].r_buff.buff.rcv.last_seq = SM[i].r_buff.sequence[k];

                                            if(k == new_last_ack_slot) {
                                                break;
                                            }

                                            k = (k+1)%MAX_WINDOW_SIZE;
                                        }

                                        SM[i].r_buff.window_size -= (new_last_ack_slot - SM[i].r_buff.base + MAX_WINDOW_SIZE)%MAX_WINDOW_SIZE + 1;
                                        SM[i].r_buff.base = (new_last_ack_slot + 1)%MAX_WINDOW_SIZE;

                                        int n = send_ack(SM[i].sockfd, SM[i].peer_addr, SM[i].r_buff.last_ack, SM[i].r_buff.window_size);
                                        if(n < 0) 
                                            printf("\nR: Error in sending ACK\n");
                                        // else 
                                            // printf("\nR: Sent ACK for seq no: %d from socket: %d\n", SM[i].r_buff.last_ack, i);
                                    }
                                }

                                break;
                            }
                        }

                        if(duplicate) {
                            printf("\nR: Duplicate message: %d\n", seq);
                            int n = send_ack(SM[i].sockfd, SM[i].peer_addr, SM[i].r_buff.last_ack, SM[i].r_buff.window_size);
                            if(n < 0) 
                                printf("\nR: Error in sending ACK\n");
                        }

                        if(SM[i].r_buff.window_size == 0) {
                            SM[i].nospace = true;
                        }
                    }
                    // if message is ACK:
                    else if(type == 0) {    
                        // printf("\nS: Received ACK for seq no: %d\n", seq);

                        int rwnd = ntohs(msg.content.ack.rwnd);

                        int j = SM[i].s_buff.base;
                        for(int x = 0; x < SM[i].s_buff.window_size; x++, j=(j+1)%MAX_WINDOW_SIZE) {
                            if(SM[i].s_buff.sequence[j] == seq) {
                                int k = SM[i].s_buff.base;
                                while(true) {
                                    SM[i].s_buff.buff.snd.timeout[k] = -1;
                                    SM[i].s_buff.buff.snd.slot_empty[k] = true; 
                                    SM[i].s_buff.sequence[k] = (SM[i].s_buff.buff.snd.next_seq_num) + k;
                                    // SM[i].s_buff.buff.snd.next_seq_num = SM[i].s_buff.sequence[k];
                                    
                                    // printf("\nS: Setting next sequence number for slot: %d to %d\n", k, SM[i].s_buff.sequence[k]);

                                    if(k == j) {
                                        break;
                                    }

                                    k = (k+1)%MAX_WINDOW_SIZE;
                                }
                                SM[i].s_buff.base = (j+1)%MAX_WINDOW_SIZE;
                                break;
                            }
                        }
                        
                        // printf("\nS: Updated base: %d\n", SM[i].s_buff.base);
                        SM[i].s_buff.window_size = rwnd;
                    }
                }
                pthread_mutex_unlock(&SM[i].lock);
            }
        }
        // no file descriptor is ready
        else {
            for(int i=0; i<MAX_SOCKETS; i++) {
                pthread_mutex_lock(&SM[i].lock);

                if(SM[i].isAlloted == true) {
                    // if socket's peer is not bound
                    if(!SM[i].isBound) {
                        int ksockfd = socket(AF_INET, SOCK_DGRAM, 0);
                        if(ksockfd < 0) {
                            printf("\nR: Error in socket creation\n");
                        }
                        else {
                            if(bind(ksockfd, (struct sockaddr* )&SM[i].self_addr, sizeof(SM[i].self_addr)) < 0) {
                                printf("Port number: %d\n", ntohs(SM[i].self_addr.sin_port));
                                perror("\nR: Error in binding");
                            }
                            else {
                                SM[i].sockfd = ksockfd;
                                SM[i].isBound = true;
                                FD_SET(SM[i].sockfd, &master);
                                if(SM[i].sockfd > maxfd) {
                                    maxfd = SM[i].sockfd;
                                }

                                //isBound printf("Maxfd: %d\n", maxfd);
                            }
                        }
                    }
                    else if(SM[i].nospace && SM[i].r_buff.window_size > 0) {
                        int n = send_ack(SM[i].sockfd, SM[i].peer_addr, SM[i].r_buff.last_ack, SM[i].r_buff.window_size);
                        if(n < 0) 
                            printf("\nR: Error in sending ACK\n");
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

            time_t base_timeout = SM[i].s_buff.buff.snd.timeout[SM[i].s_buff.base];
            time_t time_diff = time(NULL) - base_timeout;
            // if timeout has occurred
            if(time_diff >= T && base_timeout != -1) {
                int j = SM[i].s_buff.base;
                for(int x = 0; x < SM[i].s_buff.window_size; x++, j=(j+1)%MAX_WINDOW_SIZE) {
                    if(SM[i].s_buff.buff.snd.timeout[j] != -1) {

                        int n = sendto(SM[i].sockfd, &SM[i].s_buff.buff.snd.buffer[j], MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&SM[i].peer_addr, sizeof(SM[i].peer_addr));
                        if(n < 0) {
                            printf("S: couldn't send message\n");
                        }
                        // printf("\nS: Sending seq_no: %d because of timeout\n\n", SM[i].s_buff.buff.snd.buffer[j].seq_num);
                        // send_message(SM[i].sockfd, SM[i].peer_addr, SM[i].s_buff.sequence[j], SM[i].s_buff.buff.snd.buffer[j]);
                        SM[i].s_buff.buff.snd.timeout[j] = time(NULL) + T;
                    }
                    else {
                        break;
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
                            printf("S: DATA %u through ksocket: %d\n", SM[i].s_buff.buff.snd.buffer[j].seq_num, i);
                            
                            printf("\nSENDER:\n-----------\n");
                            fwrite(&SM[i].s_buff.buff.snd.buffer[j].content.data.data, 1, MAX_MESSAGE_SIZE-8, stdout);
                            printf("\n-----------\n");

                            int n = sendto(SM[i].sockfd, &SM[i].s_buff.buff.snd.buffer[j], MAX_MESSAGE_SIZE, 0, (struct sockaddr *)&SM[i].peer_addr, sizeof(SM[i].peer_addr));
                            if(n < 0) {
                                printf("S: couldn't send message\n");
                            }

                            // printf("\nS: Sending because of filled slot\n\n");
                            // send_message(SM[i].sockfd, SM[i].peer_addr, SM[i].s_buff.sequence[j], SM[i].s_buff.buff.snd.buffer[j]);
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
        // printf("Garbage collector thread\n");
        for(int i=0; i<MAX_SOCKETS; i++) {
            pthread_mutex_lock(&SM[i].lock);

            if(SM[i].isAlloted && !SM[i].isClosed) {
                if(kill(SM[i].pid, 0) == -1) {
                    printf("G: Process %d terminated\n", SM[i].pid);
                    SM[i].isClosed = true;
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