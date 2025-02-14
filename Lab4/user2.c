#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ksocket.h"

#define BUFFER_SIZE 512
#define OUTPUT_FILE "received_file.txt"  // Change this to your desired output file

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

    // printf("User2 bound to source %s:%s, destination %s:%s\n", argv[1], argv[2], argv[3], argv[4]);

    // Open output file
    FILE *fp = fopen(OUTPUT_FILE, "wb");
    if (fp == NULL) {
        perror("File open failed");
        exit(1);
    }

    // Prepare source address for recvfrom
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    // Receive and write data
    char buffer[BUFFER_SIZE];
    int total_bytes_received = 0;
    ssize_t bytes_received;
    int consecutive_errors = 0;
    const int MAX_CONSECUTIVE_ERRORS = 10;  // After this many ENOMESSAGE errors, assume transfer is complete

    while (1) {
        bytes_received = k_recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&src_addr, &src_addr_len);
        
        if (bytes_received < 0) {
            if (error_var == ENOMESSAGE) {
                // No message available, wait a bit and retry
                consecutive_errors++;
                if (consecutive_errors > MAX_CONSECUTIVE_ERRORS) {
                    printf("\nNo more messages. Assuming transfer complete.\n");
                    break;
                }
                usleep(100000);  // 100ms sleep
                continue;
            } else {
                perror("k_recvfrom failed");
                break;
            }
        }

        // Reset error counter on successful receive
        consecutive_errors = 0;

        // Write received data to file
        size_t written = fwrite(buffer, 1, bytes_received, fp);
        if (written < bytes_received) {
            perror("File write failed");
            break;
        }

        total_bytes_received += bytes_received;
        printf("\rReceived %d bytes", total_bytes_received);
        fflush(stdout);
    }

    printf("\nFile transfer complete. Total bytes received: %d\n", total_bytes_received);

    // Close file and socket
    fclose(fp);
    k_close(sockfd);

    return 0;
}