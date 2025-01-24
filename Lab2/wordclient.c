/*

Assignment 2 Submission
Name: Nived Roshan Shah
Roll Number: 22CS10049
Link of the pcap file: 

*/

#include <stdio.h> 
#include <strings.h> 
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h> 
#include <stdlib.h> 
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#define PORT 5050
#define MAXLINE 1000

int main() {
    char buffer[100];

    // declare variable to store input file name, and to store potential error message to recognize when received
    char *file_name = (char*)malloc(100*sizeof(char));
    char *not_found_error = (char*)malloc(100*sizeof(char));

    printf("Enter the file name to fetch from the server: ");
    scanf("%s", file_name);

    // pre-defining not-found error message to identify when received from server
    sprintf(not_found_error, "NOTFOUND %s", file_name);

    struct sockaddr_in server_address;
    int sockfd; // socket descriptor

    // clear server_address 
    bzero(&server_address, sizeof(server_address)); 

    // 127.0.0.1 is called the loopback address (the local machine address)
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    server_address.sin_port = htons(PORT); 
    server_address.sin_family = AF_INET; 
      
    // create a socket using User Datagram Protocol  
    sockfd = socket(AF_INET, SOCK_DGRAM, 0); 

    // struct to define a timeout for the socket: if a response is not received within tv_sec seconds, 
    // the recvfrom function will not wait any longer and the control will proceed to the next line
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    // request to send datagram 
    int sent_bytes = sendto(sockfd, file_name, strlen(file_name) + 1, 0, (struct sockaddr*)&server_address, sizeof(server_address)); 
    
    // client keeps trying to send the request to the server until it is successful
    while(sent_bytes == -1) {
        sent_bytes = sendto(sockfd, file_name, strlen(file_name) + 1, 0, (struct sockaddr*)&server_address, sizeof(server_address));
        perror("Couldn't send request to server\n"); 
        printf("Trying again...\n");
        continue;
    }

    // waiting for response
    int msg_size = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
    if (msg_size < 0) {
        // Check if the error is due to timeout (no response from server)
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            printf("Server did not respond within timeout period\n");
            close(sockfd);
            return 1;
        }
    }
    buffer[msg_size] = '\0';

    if(strcmp(buffer, not_found_error) == 0) {
        printf("FILE NOT FOUND\n");
    }
    else if(strcmp(buffer, "HELLO") == 0) {
        FILE* fp = fopen("response.txt", "w");
        fprintf(fp, "%s\n", buffer);

        printf("\nRequest acknowledged, proceeding to retrieve file contents...\n\n");

        int i = 1;
        while(1) {
            // defining message to send to the server
            char *wordi = (char*)malloc(20*sizeof(char));
            sprintf(wordi, "WORD%d", i);
            
            sendto(sockfd, wordi, strlen(wordi)+1, 0, (struct sockaddr*)&server_address, sizeof(server_address));

            msg_size = recvfrom(sockfd, buffer, sizeof(buffer), 0, NULL, NULL);
            buffer[msg_size] = '\0';

            // not handling other cases assuming FINISH will always exist in the file
            if(strcmp(buffer, "FINISH") == 0) {
                break;
            }

            printf("Word %d received from server: %s\n", i, buffer);

            fprintf(fp, "%s\n", buffer);
            i++;
        }

        fclose(fp);
    }
    // in-case the first word is not HELLO.
    else {
        printf("Unknown response from server\n");
    }

    // closing the socket
    close(sockfd);

    return 0;
}