#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 5050
#define MAX_BUFF 100

void encrypt(char* file_name, char* enc_file_name, char* key) {
    FILE *fp = fopen(file_name, "r");
    FILE *fenc = fopen(enc_file_name, "w");

    char* file_content = (char*)malloc(1000*sizeof(char));
    char c, temp; 

    while(fscanf(fp, "%c", &c) != '\0') {
        if(c >= 'a' && c <= 'z') {
            temp = key[c - 'a'];
            if(temp >= 'A' && temp <= 'Z') {
                temp = temp - 'A' + 'a';
            }
        
        }
        else if(c >= 'A' && c <= 'Z') {
            temp = key[c - 'A'];
            if(temp >= 'a' && temp <= 'z') {
                temp = temp - 'a' + 'A';
            } 
        }
        else {
            temp = c;
        }
        c = temp;
        fputc(c, fenc);
    }

    fclose(fp);
}

void send_message(int client_sockfd, char* message) {
    int message_size = strlen(message);
    while(message_size > MAX_BUFF) {
        send(client_sockfd, message, MAX_BUFF, 0);
        message_size -= MAX_BUFF;
        message += MAX_BUFF;
    }
    if(message_size > 0) {
        send(client_sockfd, message, message_size, 0);
    }
    else {
        send(client_sockfd, "%%", 2, 0);
    }
}

void receive_message(char arr[], int comm_sockfd) {
    int flag = 0;
    char temp[MAX_BUFF];
    while(1) {
        recv(comm_sockfd, temp, MAX_BUFF, 0);
        for(int i=0; i<MAX_BUFF; i++) {
            if(temp[i] == '%') {
                flag = 1;
                break;
            }
        }
        strcat(arr, temp);
        if(flag) break;
    }
}

int main() {
    int server_sockfd, comm_sockfd;
    int clilen;
    struct sockaddr_in server_addr, client_addr;

    int i, key_received = 0; 
    char key[27], buf[100];
    char file_content[1000];

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(20000);

    if(bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Unable to bind local address\n");
        exit(0);
    }

    listen(server_sockfd, 5);

    while(1) {

        clilen = sizeof(client_addr);
        comm_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &clilen);

        if(comm_sockfd < 0) {
            perror("Accept error\n");
            exit(0);
        }
        printf("Connection accepted\n");
        
        pid_t pid = fork();
        if(pid == 0) {
            close(server_sockfd);

            // Get client IP and port
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);
            
            char plain_file_name[100], enc_file_name[105];
            sprintf(plain_file_name, "%s.%d.txt", client_ip, client_port);
            sprintf(enc_file_name, "%s.enc", plain_file_name);

            while(1) {
                FILE* fp = fopen(plain_file_name, "w");

                int i=0;
                while(1) {
                    int num_received = recv(comm_sockfd, buf, MAX_BUFF, 0);
                    
                    int j=0;

                    if(!key_received) {
                        while(j<num_received && (buf[j] != '\0')) {
                            key[i] = buf[j];
                            i++; j++;
                        }

                        if(buf[j] == '\0') {
                            key_received = 1;
                            key[i] = '\0';
                        }
                        else {
                            continue;
                        }
                    }

                    num_received-=j;

                    while(num_received > 0 && buf[j] != '\0') {
                        fputc(buf[j], fp);
                        j++;
                        num_received--;
                    }

                    if(buf[j] == '\0') {
                        fputc('\0', fp);
                        break;
                    }
                }

                fclose(fp);
            }

            encrypt(plain_file_name, enc_file_name, key);

            printf("FILE ENCRYPTED\n");
        }
        
    }
}