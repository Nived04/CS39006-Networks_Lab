/*
Assignment 3 Submission
Name: Nived Roshan Shah
Roll number: 22CS10049
Link of the pcap file: https://drive.google.com/drive/folders/1kcV_YUMqJitUH8T9vJsclaRTXmOlWNxz?usp=sharing

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_BUFF 100
#define CHUNK_SIZE 15

// function to use the key map to encrypt the plain text into cipher text 
void encrypt(char* file_name, char* enc_file_name, char* key, int total_size) {
    FILE *fp = fopen(file_name, "r");
    FILE *fenc = fopen(enc_file_name, "w");

    char* file_content = (char*)malloc(1000*sizeof(char));
    char c, temp; 
    int i=0;
    
    for(int i=0; i<total_size; i++) {
        c = fgetc(fp);

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
        else temp = c;

        c = temp;
        fputc(c, fenc);
    }

    fclose(fp);
    fclose(fenc);
}

// function to get the contents of the file in a string and send it in chunks
void send_in_chunks(int* comm_sockfd, FILE* fp) {
    char c; int i=0;
    char* file_content = (char*)malloc(CHUNK_SIZE*sizeof(char));

    while((c=fgetc(fp)) != EOF) {
        file_content[i] = c;
        i++;
        if(i == CHUNK_SIZE) {
            send(*comm_sockfd, file_content, CHUNK_SIZE, 0);
            i = 0;
        }
    }

    file_content[i] = '\0';
    send(*comm_sockfd, file_content, i+1, 0);
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
    server_addr.sin_port        = htons(5050);

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

        int conn_closed = 0;

        // fork to handle multiple clients
        pid_t pid = fork();
        if(pid == 0) { // child process
            close(server_sockfd);
            
            // Get client IP and port
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            int client_port = ntohs(client_addr.sin_port);

            printf("\nConnection accepted with client at port: %d\n\n", client_port);

            char plain_file_name[100], enc_file_name[105];
            sprintf(plain_file_name, "%s.%d.txt", client_ip, client_port);
            sprintf(enc_file_name, "%s.enc", plain_file_name);

            // size of the file to be received
            int total_size = 0;

            while(1){

                while(1) {
                    FILE* fp = fopen(plain_file_name, "w");
                    int flag = 0, i=0; 
                    key_received = 0;

                    // receive until the null character is received (means end of file)
                    while(1) {
                        int num_received = recv(comm_sockfd, buf, MAX_BUFF, 0);
                        int j=0;
                        
                        // first 26 characters that will be received, make the key
                        if(!key_received) {
                            while(j<num_received && (buf[j] != '\0')) {
                                key[i] = buf[j];
                                i++; j++;
                            }

                            if(buf[j] == '\0') {
                                key_received = 1;
                                key[i] = '\0';
                                j++;
                                printf("Received Substitution Cipher key: %s\n", key);
                            }
                            else continue;
                        }

                        // if the key is received, then the rest of the message is the file content
                        num_received-=j; // -j becaseu some part of key may be with the file.
                        total_size += num_received;

                        // printf("\n%d\n", num_received);
                        
                        while(num_received > 0) {
                            fflush(NULL);
                            fputc(buf[j], fp);
                            j++;
                            num_received--;
                            if(buf[j] == '\0') {
                                flag = 1;
                                break;
                            }
                        }
                        
                        if(flag) break;
                    }

                    fclose(fp);
                    if(flag) break;
                }

                encrypt(plain_file_name, enc_file_name, key, total_size-1);
                printf("File encryption done\n");
            
                FILE* fenc = fopen(enc_file_name, "r");
                send_in_chunks(&comm_sockfd, fenc);
                fclose(fenc);

                int x = recv(comm_sockfd, buf, 1, 0);
                if(strcmp(buf, "N") == 0) {
                    shutdown(comm_sockfd, SHUT_RDWR);
                    close(comm_sockfd);
                    exit(0);
                }   

            }
        }
        else {
            close(comm_sockfd);
        }
    }
}