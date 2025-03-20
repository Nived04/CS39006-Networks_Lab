/*
Assignment 3 Submission
Name  : Nived Roshan Shah
RollNo: 22CS10049
*/

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

// message that stores the directive (task completed, process previous task, etc)
// and a type that is analogous to "status code" to indicate the situation
typedef struct {
    int type;
    char data[50];
}message;

message response;

void send_message(int type, char data[], int sockfd) {
    message msg;
    msg.type = type;
    strcpy(msg.data, data);
    for(int i=strlen(msg.data); i<50; i++) {
        msg.data[i] = '\0';
    }
    
    int n = send(sockfd, &msg, sizeof(message), 0);
    if(n < 0) {
        perror("send error");
        exit(1);
    }
}

int processTask(char task[]) {
    int op1, op2; char op;
    sscanf(task, "%d %c %d", &op1, &op, &op2);
    int result;

    switch(op) {
        case '+':
            result = op1 + op2;
            break;
        case '-':
            result = op1 - op2;
            break;
        case '*':
            result = op1 * op2;
            break;
        case '/':
            result = op1 / op2;
            break;
    }

    response.type = 202;
    sprintf(response.data, "RESULT %d", result);
    for(int i=strlen(response.data); i<50; i++) {
        response.data[i] = '\0';
    }
    
    return result;
}

int main() {
    int client_sockfd;
    struct sockaddr_in server_addr;

    response.type = -1;
    bzero(response.data, 50);

    if((client_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(5050);
    inet_aton("127.0.0.1", &server_addr.sin_addr);

    if ((connect(client_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
        perror("Unable to connect to server\n");
        exit(0);
    }

    printf("Connected to server\n");

    printf("Press 1 to ask for a new task, 2 to send computed result (if processed), or 0 to finish, when prompted\n");
    int result;

    while(1) {
        int choice;
        printf("Enter Choice: ");
        scanf(" %d", &choice);

        if(!choice) break;

        message send_msg;

        // if sending the result of computed task
        if(choice == 2) {
            if(response.type == -1) {
                printf("Please request a task from the server first\n");
                continue;
            }
            send_msg = response;
            response.type = -1;
            bzero(response.data, 50);

            int n = send(client_sockfd, &send_msg, sizeof(message), 0);
            if(n < 0) {
                perror("Error sending request\n");
                exit(0);
            }

            printf("Result %d sent\n", result);
        }
        // if requesting a new task
        else {
            send_message(201, "GET TASK", client_sockfd);
            
            message msg;
            int n = recv(client_sockfd, &msg, sizeof(message), MSG_WAITALL);
            if(n < 0) {
                perror("Error receiving task\n");
                exit(0);
            }
    
            printf("Message Type, Content: %d, %s\n", msg.type, msg.data);

            if(msg.type == 200) {
                printf("Task received: %s\n", msg.data);
                result = processTask(msg.data);
            } 
            else if(msg.type == 404) {
                printf("All tasks completed\n");
                break;
            }
            else if(msg.type == 405) {
                printf("Connection closed by the server, due to inactivity\n");
                close(client_sockfd);
                exit(0);
            }
            else {
                printf("Need to process received task first\n");
            }
        }
    }

    // send a exit message before closing the socket
    message exit_msg;
    exit_msg.type = 203;
    sprintf(exit_msg.data, "EXIT");
    for(int i=strlen(exit_msg.data); i<50; i++) {
        exit_msg.data[i] = '\0';
    }

    send(client_sockfd, &exit_msg, sizeof(message), 0);

    close(client_sockfd);
    return 0;
}