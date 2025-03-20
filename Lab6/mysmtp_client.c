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
    char response_message[20];
}response;

int state = 0;

int main(int argc, char* argv[]) {
    if(argc != 3) {
        printf("Usage: %s <ip> <port>\n", argv[0]);
        exit(0);
    }

    int client_sockfd;
    struct sockaddr_in server_addr;

    if((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    printf("Connected to My_SMTP server\n");

    while(1) {
        char command[100];
        printf("> ");
        scanf(" %[^\n]", command);
        response resp;

        int n = send(client_sockfd, command, 100, 0);
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
                    if(strlen(list_resp) == 1 && list_resp[0] == '~') {
                        break;
                    }
                    printf("%s", list_resp);
                    fflush(stdout);
                }
            }
        }
        else if(!memcmp(command, "HELO ", 5)) {
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
        }

    }
}   