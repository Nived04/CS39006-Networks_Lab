#include "ksocket.h"

ktp_socket* attach_ktp_socket() {
    key_t key = ftok(KEY_STRING, VAL);
    if(key == -1) {
        perror("ftok failed");
        exit(1);
    }
    int shmid = shmget(key, MAX_SOCKETS * sizeof(ktp_socket), 0777);
    // printf("shmid: %d\n", shmid);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    ktp_socket* M = (ktp_socket*)shmat(shmid, NULL, 0);
    return M;
}

int get_socket_num(int comparator, ktp_socket* M, int sockfd) {
    for(int i=0; i<MAX_SOCKETS; i++){
        pthread_mutex_lock(&M[i].lock);
        if(comparator == 1) {
            if(M[i].isAlloted == false) {
                printf("Returning index: %d\n", i);
                pthread_mutex_unlock(&M[i].lock);
                return i;
            }
        }
        else {
            if((M[i].isAlloted == true) && (M[i].sockfd == sockfd)) {
                printf("Returning index: %d\n", i);
                pthread_mutex_unlock(&M[i].lock);
                return i;
            }
        }
        pthread_mutex_unlock(&M[i].lock);
    }
    return -1;
} 

void init_sending_buffer(buffer* sb) {
    sb->base = 0;
    sb->buff.snd.next_seq_num = 1;
    sb->window_size = MAX_WINDOW_SIZE;
    sb->last_ack = 0;
    for(int i = 0; i < MAX_WINDOW_SIZE; i++) {
        sb->sequence[i] = i+1;
        sb->buff.snd.slot_empty[i] = true;
        sb->buff.snd.timeout[i] = -1;
    }
}

void init_receiving_buffer(buffer* rb) {
    rb->base = 0;  
    rb->window_size = MAX_WINDOW_SIZE;
    rb->buff.rcv.last_seq = 10;
    rb->last_ack = 0;
    for(int i = 0; i < MAX_WINDOW_SIZE; i++) {
        rb->sequence[i] = i+1;
        rb->buff.rcv.received[i] = false;
    }
}

bool dropMessage(float p) {
    float r = (float)rand() / (float)RAND_MAX;
    return r < p;
}

int k_socket(int domain, int type, int protocol) {
    printf("Called k_socket\n");
    fflush(NULL);
    if(type != SOCK_KTP) {
        // throw error saying that this function only allows sock_ktp type
        exit(1);
    }

    ktp_socket* SM = attach_ktp_socket();

    // find a free space in the socket memory
    int free_sock = get_socket_num(1, SM, -1);

    if(free_sock == -1) {
        errno = ENOSPACE;
        shmdt(SM);
        return -1;
    }

    pthread_mutex_lock(&SM[free_sock].lock);

    SM[free_sock].isAlloted = true;
    SM[free_sock].isBound = false;
    SM[free_sock].isClosed = false;
    SM[free_sock].nospace = false;
    SM[free_sock].pid = getpid();
    init_sending_buffer(&SM[free_sock].s_buff);
    init_receiving_buffer(&SM[free_sock].r_buff);
    bzero(&SM[free_sock].peer_addr, sizeof(struct sockaddr_in));
    bzero(&SM[free_sock].self_addr, sizeof(struct sockaddr_in));

    printf("Sock created with index: %d\n", free_sock);

    pthread_mutex_unlock(&SM[free_sock].lock);

    return free_sock;
}

int k_bind(int sock_index, const char *src_ip, int src_port, const char *dest_ip, int dest_port){
    printf("Called k_bind\n");
    fflush(NULL);

    ktp_socket* SM = attach_ktp_socket();   

    pthread_mutex_lock(&SM[sock_index].lock);

    SM[sock_index].self_addr.sin_family = AF_INET;
    SM[sock_index].self_addr.sin_port = htons(src_port);
    SM[sock_index].self_addr.sin_addr.s_addr = inet_addr(src_ip);

    SM[sock_index].peer_addr.sin_family = AF_INET;
    SM[sock_index].peer_addr.sin_port = htons(dest_port);
    SM[sock_index].peer_addr.sin_addr.s_addr = inet_addr(dest_ip);

    printf("Socket at index: %d, bound with src_port: %d dest_port: %d\n", sock_index, src_port, dest_port);

    pthread_mutex_unlock(&SM[sock_index].lock);

    return 0;
}

ssize_t k_sendto(int sock_index, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen){
    // printf("Called k_sendto\n");
    fflush(NULL);
    ktp_socket* SM = attach_ktp_socket();

    pthread_mutex_lock(&SM[sock_index].lock);

    struct sockaddr_in* dest = (struct sockaddr_in*)dest_addr;

    if(SM[sock_index].isAlloted == false || SM[sock_index].peer_addr.sin_addr.s_addr != dest->sin_addr.s_addr || SM[sock_index].peer_addr.sin_port != dest->sin_port){
        errno = ENOTBOUND;
        pthread_mutex_unlock(&SM[sock_index].lock);
        return -1;
    }

    for(int i=0; i<MAX_BUFFER_SIZE; i++) {
        if(SM[sock_index].s_buff.buff.snd.slot_empty[i]) {
            // printf("Entered slot %d\n", i);

            ssize_t copybytes = len + 8 < MAX_MESSAGE_SIZE ? len : MAX_MESSAGE_SIZE;
            memcpy(&SM[sock_index].s_buff.buff.snd.buffer[i].content.data.data, buf, len);
            
            SM[sock_index].s_buff.buff.snd.buffer[i].type = 1;
            SM[sock_index].s_buff.buff.snd.buffer[i].seq_num = SM[sock_index].s_buff.buff.snd.next_seq_num;
            
            // printf("k_sendto: len: %ld, copybytes: %ld, seq_num: %d\n", len, copybytes, SM[sock_index].s_buff.buff.snd.buffer[i].seq_num);

            for (int i = len; i < MAX_MESSAGE_SIZE - 8; i++){
                SM[sock_index].s_buff.buff.snd.buffer[i].content.data.data[i] = '\0';
            }
            
            SM[sock_index].s_buff.buff.snd.slot_empty[i] = false;
            SM[sock_index].s_buff.buff.snd.timeout[i] = -1;
            SM[sock_index].s_buff.buff.snd.next_seq_num = (SM[sock_index].s_buff.buff.snd.next_seq_num)%MAX_SEQ_NUM + 1;
            
            // printf("k_sendto: Message \n\n%s\n\n sent with sock_index: %d and buffer index: %d\n\n", SM[sock_index].s_buff.buff.snd.buffer[i].content.data.data, sock_index, i);
            
            pthread_mutex_unlock(&SM[sock_index].lock);
            
            return copybytes;
        }
    }

    pthread_mutex_unlock(&SM[sock_index].lock);

    errno = ENOSPACE;
    return -1;
}

ssize_t k_recvfrom(int sock_index, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
    // printf("k_recvfrom called\n");
    ktp_socket* SM = attach_ktp_socket();

    pthread_mutex_lock(&SM[sock_index].lock);

    int numbytes, slot; 
    slot = (SM[sock_index].r_buff.base + SM[sock_index].r_buff.window_size) % MAX_WINDOW_SIZE;

    // printf("k_recvfrom: Checking slot %d (base=%d, window_size=%d, received: %d)\n", slot, SM[sock_index].r_buff.base, SM[sock_index].r_buff.window_size, SM[sock_index].r_buff.buff.rcv.received[slot]);

    if(SM[sock_index].r_buff.buff.rcv.received[slot]) {
        // char* temp = (char*)buf;

        // for(int i=0; i<len; i++) {
        //     temp[i] = SM[sock_index].r_buff.buff.rcv.buffer[slot].content.data.data[i];
        // }

        memcpy(buf, SM[sock_index].r_buff.buff.rcv.buffer[slot].content.data.data, len);
        
        // printf("\n----------------\nBuffer:\n");
        // for(int i=0; i<len; i++) {
        //     printf("%c", SM[sock_index].r_buff.buff.rcv.buffer[slot].content.data.data[i]);
        // }
        // printf("\n----------------\n");

        numbytes = len;

        SM[sock_index].r_buff.buff.rcv.received[slot] = false;
        SM[sock_index].r_buff.window_size++;
    }
    else {
        numbytes = -1;
        errno = ENOMESSAGE;
    }

    pthread_mutex_unlock(&SM[sock_index].lock);
    return numbytes;
}

int k_close(int sock_index) {
    ktp_socket* SM = attach_ktp_socket();

    pthread_mutex_lock(&SM[sock_index].lock);
    SM[sock_index].isClosed = true;
    pthread_mutex_unlock(&SM[sock_index].lock);

    return 0;
}
