#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>

typedef struct {
    int type;
    char data[50];
}message;

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

    return result;
}

int main() {
    int client_sockfd;
    struct sockaddr_in server_addr;

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

    while(1) {
        int choice;
        printf("Press 1 to ask for a new task, or 0 to finish: ");
        scanf(" %d", &choice);
        if(!choice) break;

        message send_msg;
        send_msg.type = 201;
        sprintf(send_msg.data, "GET_TASK");

        int n = send(client_sockfd, &send_msg, sizeof(message), 0);
        if(n < 0) {
            perror("Error sending request\n");
            exit(0);
        }

        message msg;
        n = recv(client_sockfd, &msg, sizeof(message), MSG_WAITALL);
        if(n < 0) {
            perror("Error receiving task\n");
            exit(0);
        }

        printf("Received task_id: %d, %s\n", msg.type, msg.data);

        if(msg.type == 200) {
            printf("Task received: %s\n", msg.data);
            int result = processTask(msg.data);

            message response;
            response.type = 202;
            sprintf(response.data, "RESULT %d", result);

            n = send(client_sockfd, &response, 50, 0);
            if(n < 0) {
                perror("Error sending response\n");
                exit(0);
            }

            printf("Result %d sent\n", result);
        } 
        else if(msg.type == 404) {
            printf("All tasks completed\n");
        }
        else {
            printf("Need to process received task first\n");
        }
    }

    close(client_sockfd);
    return 0;
}