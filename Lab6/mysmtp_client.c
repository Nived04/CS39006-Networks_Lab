/*
Assignment 6 Submission
Name  : Nived Roshan Shah
RollNo: 22CS10049
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <strings.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

typedef struct {
    int status;
    char response_message[50];
}response;

int state = 0;

int main(int argc, char* argv[]) {
    if(argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(0);
    }

    int client_sockfd;
    struct sockaddr_in server_addr;
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);
    
    if((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    if(connect(client_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed\n");
        exit(0);
    }
    
    printf("Connected to My_SMTP server\n");

    while(1) {
        char command[100];
        printf("> ");
        scanf(" %[^\n]s", command);

        int l = strlen(command);
        command[l] = '\r';
        for(int i=l+1; i<100; i++) {
            command[i] = '\0';
        }

        response resp;

        int n = send(client_sockfd, command, 100, 0);

        // printf("%d\n", n);

        if(n < 0) {
            perror("Error sending request\n");
            exit(0);
        }

        if(!memcmp(command, "LIST ", 5)) {
            int x = recv(client_sockfd, &resp, sizeof(response), MSG_WAITALL);
            if(x < 0) {
                perror("Error receiving response\n");
                exit(0);
            }
            printf("%d %s\n", resp.status, resp.response_message);
            char list_resp[200];
            if(resp.status == 200) {
                while(1) {
                    x = recv(client_sockfd, &list_resp, 200, MSG_WAITALL);
                    if(x < 0) {
                        perror("Error receiving response\n");
                        exit(0);
                    }
                    // detect list terminator
                    if(strlen(list_resp) == 1 && list_resp[0] == '~') {
                        break;
                    }
                    printf("%s", list_resp);
                    fflush(stdout);
                }
            }
        }
        // only status and message is received for the following commands
        else if(!memcmp(command, "HELO ", 5) || 
                !memcmp(command, "MAIL FROM: ", 11) || 
                !memcmp(command, "RCPT TO: ", 9) || 
                !memcmp(command, "QUIT", 4)) {
            while(1) {
                int x = recv(client_sockfd, &resp, sizeof(response), MSG_WAITALL);
                if(x < 0) {
                    perror("Error receiving response\n");
                    exit(0);
                }
                else {
                    printf("%d %s\n", resp.status, resp.response_message);
                    break;
                }
            }
            
            if(!memcmp(command, "QUIT", 4)) {
                break;
            }
        }
        else if(!memcmp(command, "DATA", 4)) {
            int x = recv(client_sockfd, &resp, sizeof(response), MSG_WAITALL);
            if(resp.status != 200) {
                printf("%d %s\n", resp.status, resp.response_message);
                continue;
            }

            printf("Enter your message (end with a single dot '.' on a new line):\n");
            char message[10000] = "";
            char line[100];
            
            while(1) {
                scanf(" %[^\n]s", line);
                strcat(message, line);
                strcat(message, "\n");
                if(strcmp(line, ".") == 0) {
                    break;
                }
            }
            
            n = send(client_sockfd, message, strlen(message), 0);
            if(n < 0) {
                perror("Error sending data\n");
                exit(0);
            }
            
            x = recv(client_sockfd, &resp, sizeof(response), MSG_WAITALL);
            if(x < 0) {
                perror("Error receiving response\n");
                exit(0);
            }
            printf("%d %s\n", resp.status, resp.response_message);
        }
        else if(!memcmp(command, "GET_MAIL ", 9)) {
            int x = recv(client_sockfd, &resp, sizeof(response), MSG_WAITALL);
            if(x < 0) {
                perror("Error receiving response\n");
                exit(0);
            }
            printf("%d %s\n", resp.status, resp.response_message);
            
            if(resp.status == 200) {
                char mail_content[10000];
                x = recv(client_sockfd, mail_content, 10000, 0);
                if(x < 0) {
                    perror("Error receiving mail content\n");
                    exit(0);
                }
                printf("%s", mail_content);
            }
        }
        else {
            int x = recv(client_sockfd, &resp, sizeof(response), MSG_WAITALL);
            if(x < 0) {
                perror("Error receiving response\n");
                exit(0);
            }
            printf("%d %s\n", resp.status, resp.response_message);
        }
    }
    
    close(client_sockfd);
    return 0;
}