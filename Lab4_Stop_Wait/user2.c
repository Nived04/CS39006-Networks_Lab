/*
Assignment 3 (Preliminary) Submission
Name: Nived Roshan Shah
Roll Number: 22CS10049
*/

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
#define P 0.1

int expected_seq_no = 1;

struct message {
    int type;
    int seq;
    char data[504];
};

int dropMessage(float p) {
    float random = (float)rand() / RAND_MAX;
    return (random < p) ? 1 : 0;
}

int main() {
    struct sockaddr_in addr;
    int sockfd;

    bzero(&addr, sizeof(addr));

    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(PORT2);
    addr.sin_family = AF_INET;

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

    struct sockaddr_in sender_addr;
    int sender_len = sizeof(sender_addr);

    fd_set readfds;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct message msg;

        int ready = select(sockfd + 1, &readfds, NULL, NULL, NULL);

        if(ready == -1) {
            perror("Error in select");
            close(sockfd);
            return 1;
        }

        if(FD_ISSET(sockfd, &readfds)) {
            int recv_bytes = recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&sender_addr, &sender_len);

            if(recv_bytes == -1) {
                perror("Error in recvfrom");
                close(sockfd);
                return 1;
            }

            if(msg.type == 1 && msg.seq == expected_seq_no && !dropMessage(P)) {
                expected_seq_no++;
                // printf("Received data:\n\n%s\n\n", msg.data);
                
                struct message ack;
                ack.type = 0;
                ack.seq = msg.seq;
    
                int sent_bytes = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&sender_addr, sender_len);
    
                if(sent_bytes == -1) {
                    perror("Error in sendto");
                    close(sockfd);
                    return 1;
                }
                
                printf("Sent ACK for sequence number: %d\n", ack.seq);
            }
            else if(msg.seq == expected_seq_no - 1) {
                printf("Received duplicate data with sequence number: %d (ack may have been lost)\n", msg.seq);
                struct message ack;
                ack.type = 0;
                ack.seq = msg.seq;
    
                int sent_bytes = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&sender_addr, sender_len);
    
                if(sent_bytes == -1) {
                    perror("Error in sendto");
                    close(sockfd);
                    return 1;
                }
                
                printf("Re-sent ACK for sequence number: %d\n", ack.seq);
            }
        }
    }
    return 0;
}