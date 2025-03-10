/*
Assignment 3 (Preliminary) Submission
Name: Nived Roshan Shah
Roll Number: 22CS10049
*/

#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include "ksocket.h"

int dropMessage(float p) {
    float random = (float)rand() / RAND_MAX;
    return (random < p) ? 1 : 0;
}

int main() {
    int sockfd;

    sockfd = k_socket(AF_INET, SOCK_KTP, 0);

    if(sockfd < 0) {
        perror("Error creating socket");
        return 1;
    }

    if(k_bind(sockfd, "127.0.0.1", 5051, "127.0.0.1", 5050) < 0) {
        printf("Error binding socket\n");
        k_close(sockfd);
        return 1;
    }

    struct sockaddr_in sender_addr;
    int sender_len = sizeof(sender_addr);
    struct message msg;

    int rec_bytes = k_recvfrom(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&sender_addr, &sender_len);

    if(error_var == ENOMESSAGE) {
        k_close(sockfd);
        return 0;
    }

    return 0;
}