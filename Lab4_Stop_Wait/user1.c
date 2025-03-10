/*
Assignment 3 (Preliminary) Submission
Name: Nived Roshan Shah
Roll Number: 22CS10049
*/

// Reads and sends the content of the largeFile.txt file to user2.c (with total message size of 512 bytes)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdbool.h>
#include <unistd.h>
#include <time.h>

#define PORT1 5050
#define PORT2 5051
#define T 5

#define MSG_DATA 1
#define MSG_ACK 0

#define P 0.3

int dropMessage(float p) {
    float random = (float)rand() / RAND_MAX;
    return (random < p) ? 1 : 0;
}

struct message {
    int type;
    int seq;
    char data[504];
};

int next_seq_no = 1;

int main() {
    struct sockaddr_in addr;
    int sockfd;

    bzero(&addr, sizeof(addr));

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT1);
    addr.sin_family = AF_INET;

    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if(sockfd < 0) {
        perror("Error creating socket");
        return 1;
    }

    if(bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("Error binding socket");
        close(sockfd);
        return 1;
    }
    
    // defining destination address
    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    dest_addr.sin_port = htons(PORT2);
    dest_addr.sin_family = AF_INET;
    
    FILE* file = fopen("largeFile.txt", "r");
    
    fd_set readfds;
    int max_tries = 10;
    
    int i = 0, j = 0;
    while(i<max_tries || j<max_tries) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = T;
        timeout.tv_usec = 0;
        
        bool ack_received = false;

        struct message msg;
        bzero(&msg, sizeof(msg));
        
        msg.type = MSG_DATA;
        msg.seq = next_seq_no;

        int read_bytes = fread(msg.data, 1, 504, file);
        int sent_bytes = sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if(activity == 0) {
            printf("Timeout occurred, resending message\n");
            fseek(file, -read_bytes, SEEK_CUR);
            i++;
            continue;
        }
        
        if(FD_ISSET(sockfd, &readfds)) {
            struct message ack;
            bzero(&ack, sizeof(ack));

            int rec_bytes = recvfrom(sockfd, &ack, sizeof(ack), 0, NULL, NULL);

            if(rec_bytes < 0) {
                perror("Error receiving ack");
                close(sockfd);
                return 1;
            }

            if(!dropMessage(P)){
                if(ack.type == 0 && ack.seq == msg.seq) {
                    printf("ACK received for seq %d\n", ack.seq);
                    next_seq_no++;
                    if(feof(file)) {
                        printf("Finished sending file\n");
                        break;
                    }
                }
            }
            else {
                printf("ACK not received for seq %d\n", msg.seq);
                fseek(file, -read_bytes, SEEK_CUR);
                j++;
            }

        }
    }

    return 0;
}