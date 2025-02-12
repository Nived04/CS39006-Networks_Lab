#include "ksocket.h"
#include <sys/shm.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SOCKET_NUMBER 10
#define WINDOW_SIZE 10
#define MESSAGE_SIZE 512
#define KEY_STRING "/"
#define VAL 65

typedef struct {
    int window_size;
    int not_acked[WINDOW_SIZE];
}window;

typedef struct {
    bool isAlloted;
    int pid;
    int udp_sockfd;
    int peer_port;
    char peer_ip[16];
    char send_buffer[10][MESSAGE_SIZE];
    char receive_buffer[10][MESSAGE_SIZE];
    int send_buffer_count;
    int receive_buffer_count;
    window swnd;
    window rwnd;
}socket_shm;

socket_shm* attach_socket_shm() {
    key_t key = ftok(KEY_STRING, VAL);
    int shmid = shmget(key, SOCKET_NUMBER * sizeof(socket_shm), 0777);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    socket_shm* M = (socket_shm*)shmat(shmid, NULL, 0);
    return M;
}

int get_socket_num(int comparator, socket_shm* M, int sockfd) {
    int i=0;
    while(i < SOCKET_NUMBER) {
        if(comparator == 1) {
            if(M[i].isAlloted == false) {
                return i;
            }
        }
        else {
            if((M[i].isAlloted == true) && (M[i].udp_sockfd == sockfd)) {
                return i;
            }
        }
        i++;
    }
    return -1;
} 

int k_socket(int domain, int type, int protocol) {
    if(type != SOCK_KTP) {
        // throw error saying that this function only allows sock_ktp type
        exit(1);
    }

    socket_shm* M = attach_socket_shm();
    int curr;

    if((curr = get_socket_num(1, M, -1)) == -1) {
        error_var = ENOSPACE;
        shmdt(M);
        return -1;
    }

    int sockfd = socket(domain, SOCK_DGRAM, protocol);
    if(sockfd == -1) {
        // error in socket creation
        shmdt(M);
        return -1;
    }

    M[curr].isAlloted = true;
    M[curr].pid = getpid();
    M[curr].udp_sockfd = sockfd;
    M[curr].send_buffer_count = 0;
    M[curr].receive_buffer_count = 0;
    M[curr].swnd.window_size = WINDOW_SIZE;
    M[curr].rwnd.window_size = WINDOW_SIZE;

    shmdt(M);
    return sockfd;
}

int k_bind(int sockfd, char* source_ip, int source_port, char* dest_ip, int dest_port) {
    socket_shm* M = attach_socket_shm();
    int curr;

    if((curr = get_socket_num(2, M, sockfd)) == -1) {
        // throw error saying that the sockfd given is not valid
        shmdt(M);
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(source_port);
    addr.sin_addr.s_addr = inet_addr(source_ip);

    int result = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if (result == 0) {
        strncpy(M[curr].peer_ip, dest_ip, 16);
        M[curr].peer_port = dest_port;
    }

    shmdt(M);
    return result;
}

ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    socket_shm* M = attach_socket_shm();
    int curr;
    
    if((curr = get_socket_num(2, M, sockfd)) == -1) {
        // throw error saying that the sockfd given is not valid
        shmdt(M);
        return -1;
    }

    struct sockaddr_in* dest = (struct sockaddr_in*)dest_addr;
    char *dest_ip = inet_ntoa(dest->sin_addr);
    int dest_port = ntohs(dest->sin_port); 

    if((strcmp(M[curr].peer_ip, dest_ip) != 0) || (M[curr].peer_port != dest_port)) {
        error_var = ENOTBOUND;
        shmdt(M);
        return -1;
    }

    if(M[curr].send_buffer_count >= WINDOW_SIZE) {
        shmdt(M);
        error_var = ENOSPACE;
        return -1;
    }
        
    memcpy(M[curr].send_buffer[M[curr].send_buffer_count], buf, len);
    M[curr].send_buffer_count++;

    shmdt(M);
    return len;
}

ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) {
    socket_shm* M = attach_socket_shm();
    int curr;

    if((curr = get_socket_num(2, M, sockfd)) == -1) {
        // throw error saying that the sockfd given is not valid
        shmdt(M);
        return -1;
    }

    if (M[curr].receive_buffer_count == 0) {
        // pthread_mutex_unlock(&M[curr].lock);
        shmdt(M);
        error_var = ENOMESSAGE;
        return -1;
    }

    size_t copy_len = (len < MESSAGE_SIZE) ? len : MESSAGE_SIZE;
    memcpy(buf, M[curr].receive_buffer[0], copy_len);

    // Shift remaining messages in buffer
    for (int i = 0; i < M[curr].receive_buffer_count - 1; i++) {
        memcpy(M[curr].receive_buffer[i], M[curr].receive_buffer[i + 1], MESSAGE_SIZE);
    }
    M[curr].receive_buffer_count--;

    // Set source address if requested
    if (src_addr != NULL && addrlen != NULL) {
        struct sockaddr_in* src = (struct sockaddr_in*)src_addr;
        src->sin_family = AF_INET;
        src->sin_port = htons(M[curr].peer_port);
        src->sin_addr.s_addr = inet_addr(M[curr].peer_ip);
        *addrlen = sizeof(struct sockaddr_in);
    }

    // pthread_mutex_unlock(&M[curr].lock);
    shmdt(M);
    return copy_len;
}

int k_close(int sockfd) {
    socket_shm* M = attach_socket_shm();
    int curr = get_socket_num(2, M, sockfd);
    
    if (curr == -1) {
        shmdt(M);
        // errno = EBADF;
        return -1;
    }

    // pthread_mutex_lock(&M[curr].lock);

    // Close UDP socket
    close(sockfd);

    // Clear socket entry
    M[curr].isAlloted = false;
    M[curr].pid = 0;
    M[curr].udp_sockfd = -1;
    M[curr].send_buffer_count = 0;
    M[curr].receive_buffer_count = 0;

    // pthread_mutex_unlock(&M[curr].lock);
    shmdt(M);
    return 0;
}