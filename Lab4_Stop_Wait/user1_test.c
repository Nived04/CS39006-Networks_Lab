/*
Assignment 3 (Preliminary) Submission
Name: Nived Roshan Shah
Roll Number: 22CS10049
*/

// Reads and sends the content of the largeFile.txt file to user2.c (with total message size of 512 bytes)

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "ksocket.h"

#define MSG_DATA 1
#define MSG_ACK 0

int next_seq_no = 1;

int main() {
    int sockfd;

    struct timeval tv;
    tv.tv_sec = T;
    tv.tv_usec = 0;

    sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if(sockfd < 0) {
        perror("Error creating socket");
        return 1;
    }

    if(k_bind(sockfd, "127.0.0.1", 5050, "127.0.0.1", 5051) < 0) {
        perror("Error binding socket");
        k_close(sockfd);
        return 1;
    }
    
    FILE* file = fopen("largeFile.txt", "r");
    
    fd_set readfds;
    int max_tries = 10;
    int i = 0;

    while(i<max_tries) {
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

            if(ack.type == 0 && ack.seq == msg.seq) {
                printf("ACK received for seq %d\n", ack.seq);
                next_seq_no++;
                if(feof(file)) {
                    printf("Finished sending file\n");
                    break;
                }
            }

        }
    }

    return 0;
}