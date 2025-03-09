#include <stdio.h>
#include <ksocket.h>

char msg[504];
const char *eof_marker = "~";

int main(int argc, char *argv[]) {
    if(argc != 5){
        printf("Enter Src IP, Src Port, Dest IP and Dest Port in that order\n");
        return -1;
    }

    const char *src_ip = argv[1];
    int src_port = atoi(argv[2]);
    const char *dest_ip = argv[3];
    int dest_port = atoi(argv[4]);

    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if(sockfd < 0) {
        if(errno == ENOSPACE) {
            printf("user1: k_socket: No space for new socket\n");
        }
        else {
            perror("user1: k_socket");
        }
        return -1;
    }

    if ((k_bind(sockfd, src_ip, src_port, dest_ip, dest_port)) < 0) {
        perror("user2: k_bind");
        return -1;
    }

    char file_name[100];
    sprintf(file_name, "received_%d.txt", src_port);
    FILE *fp = fopen(file_name, "w");
    if (fp == NULL) {
        printf("user1: Error in opening file\n");
        return -1;
    }

    sleep(2);

    while (1) {
        int bytesRecv = k_recvfrom(sockfd, msg, MAX_MESSAGE_SIZE-8, 0, NULL, 0);

        if (bytesRecv < 0) {
            if (errno == ENOMESSAGE) {
                sleep(1);
                continue;
            }
            fclose(fp);
            k_close(sockfd);
            return -1;
        }

        printf("\nuser2: --- Received Message ---\n");
        for(int i=0; i<bytesRecv; i++) {
            printf("%c", msg[i]);
        }
        printf("\n");
        printf("--- of %d bytes ---\n\n", bytesRecv);

        if (memcmp(msg, eof_marker, 1) == 0) {
            printf("user2: \"finished_reading\" marker received\n");
            break;
        }

        printf("user2: Writing content to file\n");
        for(int i=0; i<bytesRecv; i++) {
            if(msg[i] == '\0') {
                continue;
            }
            fputc(msg[i], fp);
        }
        fflush(fp);
    }

    fclose(fp);

    k_close(sockfd);
    
    return 0;
}