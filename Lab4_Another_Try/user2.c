#include <stdio.h>
#include <ksocket.h>

char msg[504];
const char *eof_marker = "~";

int main(int argc, char *argv[]) {
    if(argc != 5){
        printf("Usage: %s <src_ip> <src_port> <dest_ip> <dest_port>\n", argv[0]);
        return -1;
    }

    const char *src_ip = argv[1];
    int src_port = atoi(argv[2]);
    const char *dest_ip = argv[3];
    int dest_port = atoi(argv[4]);

    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sockfd < 0) {
        perror("user2: k_socket");
        return -1;
    }

    if ((k_bind(sockfd, src_ip, src_port, dest_ip, dest_port)) < 0) {
        perror("user2: k_bind");
        return -1;
    }

    // FILE *fp = fopen("received_10KB.txt", "w");
    FILE *fp = fopen("received_10KB.txt", "w");
    if (fp == NULL) {
        perror("user2: fopen");
        return -1;
    }

    sleep(2);
    while (1) {
        int n = k_recvfrom(sockfd, msg, MAX_MESSAGE_SIZE-8, 0, NULL, 0);
        if (n < 0) {
            if (errno == ENOMESSAGE) {
                sleep(1);
                continue;
            }
            fclose(fp);
            k_close(sockfd);
            return -1;
        }

        printf("user2: received %d bytes\n", n);

        if (memcmp(msg, eof_marker, 1) == 0) {
            printf("user2: EOF marker received\n");
            break;
        }

        printf("user2: writing %d bytes to file", n);
        for(int i=0; i<n; i++) {
            if(msg[i] == '\0') {
                continue;
            }
            fputc(msg[i], fp);
            // printf("%c", msg[i]);
        }
        fflush(fp);
    }

    fclose(fp);

    k_close(sockfd);
    
    return 0;
}