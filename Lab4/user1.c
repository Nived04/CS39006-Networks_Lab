/*
Assignment 4 Submission
Name: Nived Roshan Shah
RollNo: 22CS10049
*/

#include <stdio.h>
#include <ksocket.h>

char msg_data[504];
const char *eof_marker = "~";

int read_bytes(FILE* fp, int bytes) {
    bzero(&msg_data, bytes);
    int cnt = 0;
    while((cnt < bytes)) {
        msg_data[cnt] = fgetc(fp);
        cnt++;
        if(msg_data[cnt-1] == EOF) {
            cnt--;
            break;
        }
    }
    return cnt;
}

void send_message(int sockfd, struct sockaddr_in addr, char* msg, int len, FILE* fp) {
    int numbytes;
    while(1) {
        numbytes = k_sendto(sockfd, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr));
        
        if (numbytes < 0) {
            if(errno == ENOSPACE) {
                printf("user1: k_send: No space for new message, trying again...\n");
                sleep(1);
            }
            else {
                perror("user1: k_send");
                fclose(fp);
                k_close(sockfd);
                exit(-1);
            }
        }
        else break;
    }

    printf("user1: Sent %d bytes\n", numbytes);
}

int main(int argc, char *argv[])
{
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

    if((k_bind(sockfd, src_ip, src_port, dest_ip, dest_port)) < 0) {
        perror("user1: k_bind");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);
    addr.sin_addr.s_addr = inet_addr(dest_ip);

    // FILE *fp = fopen("lorem_1KB.txt", "r");
    // FILE *fp = fopen("lorem_10KB.txt", "r");
    FILE *fp = fopen("lorem_100KB.txt", "r");
    if(fp == NULL) {
        printf("user1: Error in opening file\n");
        return -1;
    }

    sleep(2);

    while (1) {
        int bytesRead = read_bytes(fp, MAX_MESSAGE_SIZE - 8);

        printf("\nuser1: --- Read Message ---\n");
        for(int i=0; i<bytesRead; i++) {
            printf("%c", msg_data[i]);
        }
        printf("\n");
        printf("--- of %d bytes ---\n\n", bytesRead);

        send_message(sockfd, addr, msg_data, bytesRead, fp);

        if(feof(fp)) {
            printf("user1: End of file reached. Sending a \"finished_reading\" marker (~)\n");
            char msg_end[1] = "~";
            send_message(sockfd, addr, msg_end, 1, fp);
            break;
        }
    }

    fclose(fp);

    sleep(20);

    k_close(sockfd);
    return 0;
}