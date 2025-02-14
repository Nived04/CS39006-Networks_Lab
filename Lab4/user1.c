#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ksocket.h"

#define BUFFER_SIZE 512
#define FILE_PATH "largeFile.txt"  // Change this to your file path

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <source_ip> <source_port> <dest_ip> <dest_port>\n", argv[0]);
        exit(1);
    }

    // Create socket
    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sockfd < 0) {
        perror("k_socket failed");
        exit(1);
    }

    // Bind socket with source and destination
    if (k_bind(sockfd, argv[1], atoi(argv[2]), argv[3], atoi(argv[4])) < 0) {
        perror("k_bind failed");
        exit(1);
    }

    // Open file
    FILE *fp = fopen(FILE_PATH, "rb");
    if (fp == NULL) {
        perror("File open failed");
        exit(1);
    }

    // Prepare destination address
    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(atoi(argv[4]));
    dest_addr.sin_addr.s_addr = inet_addr(argv[3]);

    // Read and send file
    char buffer[BUFFER_SIZE];
    size_t bytes_read;
    int total_bytes_sent = 0;
    
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        ssize_t sent = k_sendto(sockfd, buffer, bytes_read, 0, 
                               (struct sockaddr*)&dest_addr, sizeof(dest_addr));
        
        if (sent < 0) {
            if (error_var == ENOSPACE) {
                // Buffer full, wait a bit and retry
                printf("Send buffer full, waiting...\n");
                sleep(1);
                continue;
            } else if (error_var == ENOTBOUND) {
                printf("Destination not bound\n");
                break;
            } else {
                perror("k_sendto failed");
                break;
            }
        }
        
        total_bytes_sent += sent;
        printf("\rSent %d bytes", total_bytes_sent);
        fflush(stdout);
    }

    printf("\nFile transfer complete. Total bytes sent: %d\n", total_bytes_sent);

    // Close file and socket
    fclose(fp);
    k_close(sockfd);

    return 0;
}