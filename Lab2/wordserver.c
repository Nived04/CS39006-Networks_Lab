/*

Assignment 2 Submission
Name: Nived Roshan Shah
Roll Number: 22CS10049
Link of the pcap file: 

*/

#include <stdio.h> 
#include <strings.h> 
#include <string.h>
#include <sys/types.h> 
#include <arpa/inet.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#define PORT 5050
#define MAXLINE 1000 
  
int main() {    
    char buffer[100]; 
        
    int serverfd; // server socket descriptor
    socklen_t len; // length of socket address
    struct sockaddr_in server_address, client_address; 
    
    bzero(&server_address, sizeof(server_address)); 

    // Create a UDP Socket 
    serverfd = socket(AF_INET, SOCK_DGRAM, 0); 
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); 
    server_address.sin_port = htons(PORT);
    server_address.sin_family = AF_INET;  
   
    // bind server address to socket descriptor 
    bind(serverfd, (struct sockaddr*)&server_address, sizeof(server_address)); 
    
    struct timeval tv;
    tv.tv_sec = 15;
    tv.tv_usec = 0;
    setsockopt(serverfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

    while(1) {
        printf("\nServer Running .........\n\n");
    
        //receive the datagram 
        len = sizeof(client_address);
        
        int msg_size = recvfrom(serverfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_address, &len); //receive message from server 
        buffer[msg_size] = '\0'; 

        if(msg_size < 0) {
            if(errno == EWOULDBLOCK || errno == EAGAIN) {
                printf("Client did not respond within timeout period\n");
                close(serverfd);
                return 1;
            }
            else {
                perror("Error receiving response");
                close(serverfd);
                return 1;
            }
        }

        FILE* fp = fopen(buffer, "r");
        
        if(fp == NULL) {
            char *not_found_error = (char*)malloc(110*sizeof(char));
            sprintf(not_found_error, "NOTFOUND %s", buffer);

            sendto(serverfd, not_found_error, strlen(not_found_error) + 1, 0, (struct sockaddr*)&client_address, sizeof(client_address));

            printf("Requested file - %s - not found in the server\n", buffer);
        }
        else {
            char *wordi = (char*)malloc(MAXLINE*sizeof(char));
            int i=1;
            while(fscanf(fp, "%s", wordi) != 0) {
                
                int sent_bytes = sendto(serverfd, wordi, strlen(wordi) + 1, 0, (struct sockaddr*)&client_address, sizeof(client_address));
                
                if(sent_bytes == -1) {
                    printf("***Error in sending message, trying again...\n");
                    rewind(fp);
                    continue;
                }
                
                printf("Word %d sent: %s\n", i, wordi);
                
                if(strcmp(wordi, "FINISH") == 0) {
                    break;
                }

                bzero(wordi, MAXLINE);

                int msg_size = recvfrom(serverfd, buffer, sizeof(buffer), 0, (struct sockaddr*)&client_address, &len);
                buffer[msg_size] = '\0';
                i++;
            }

            printf("\nFile contents sent to client\n\n");
        }
    }

    return 0;
}