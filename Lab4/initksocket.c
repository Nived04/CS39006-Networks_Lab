#include <sys/shm.h>	
#include <stdlib.h>
#include <stdbool.h>

#define SOCKET_NUMBER 10
#define WINDOW_SIZE 10
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
    char* peer_ip;
    char send_buffer[10][512];
    char receive_buffer[10][512];
    window swnd;
    window rwnd;
}socket_shm;

int main() {
    key_t key;
    key = ftok(KEY_STRING, VAL);
    int shmid = shmget(key, SOCKET_NUMBER*sizeof(socket_shm), 0777|IPC_CREAT|IPC_EXCL);
    socket_shm* M = (socket_shm*)shmat(shmid, 0, 0);
    
    for(int i=0; i<SOCKET_NUMBER; i++) {
        M[i].isAlloted = false;
        M[i].swnd.window_size = WINDOW_SIZE;
        M[i].rwnd.window_size = WINDOW_SIZE;
    }

}