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

#define HELO "HELO "
#define MAIL_FROM "MAIL FROM: "
#define RCPT_TO "RCPT TO: "
#define DATA "DATA "
#define LIST "LIST "
#define GET_MAIL "GET_MAIL "
#define QUIT "QUIT"

int MY_SMTP_PORT;

typedef struct {
    int status;
    char response_message[20];
}response;

typedef struct {
    int id;
    char from[30];
    char date[11];
    char content[10000];
}mail;

void sendResponse(int status, char message[], int sockfd) {
    response resp;
    resp.status = status;
    strncpy(resp.response_message, message, 20);

    send(sockfd, &resp, sizeof(response), 0);
}

char* getName(char mail[]) {
    char name[30];
    int flag = 0;
    for(int i=0; i<30; i++) {
        if(mail[i] == '@')
            flag = 1;
        if(!flag)
            name[i] = mail[i];
        else {
            name[i] = '\0';
        }
    }
    return name;  
}

void create_mailbox(char domain[], char from[], char to[], int fd) {
    char name[30] = getName(to);
    char filename[30];
    sprintf(filename, "%s.txt", name);
    FILE* f = fopen(filename, "r");
    fclose(f);
    return;
}

int store_message(char from[], char to[], char content[], int *rec_cnt) {
    char date[11];
    struct tm *current_dt = localtime(time(NULL));
    sprintf(date, "%2d-%2d-%4d", current_dt->tm_mday, current_dt->tm_mon+1, current_dt->tm_year+1900);
    date[10] = '\0';
    char name[30] = getName(to);
    char filename[30];
    sprintf(filename, "%s.txt", name);
    FILE* f = fopen(filename, "a");

    if(f == NULL) {
        perror("Error opening file");
        return -1;
    }

    int l = strlen(content);
    // since content ends with a . on a new line, we remove the last two characters
    *rec_cnt += 1;
    content[l-2] = '\0';
    content[l-1] = '\0';
    if(fprintf(f, "\n-----\nMail_ID: %d\nFrom: %s\nDate: %s\n%s\n", *rec_cnt, from, date, content) < 0) {
        perror("Error writing to file");
        *rec_cnt -= 1;
        return -1;
    }

    fclose(f);
    return 0;
}

void sendList(char mail[], int sockfd) {
    char name[30] = getName(mail);
    char filename[30];
    sprintf(filename, "%s.txt", name);
    FILE* f = fopen(filename, "r");

    if(f == NULL) {
        perror("Error opening file");
        return;
    }
    
    char list_resp[200];
    while(1) {  
        bzero(list_resp, 200);
        char line[10];
        while(memcmp(fgets(line, 7, f), "\n-----\n", 7)) {
            if(feof(f)) {
                break;
            }
        }

        if(feof(f)) break;
        fseek(f,5,SEEK_CUR);
        int id;
        fseek(f, 9, SEEK_CUR);
        fscanf(f, "%d", &id);
        // get the from field
        fseek(f, 6, SEEK_CUR);
        char from[30];
        fscanf(f, "%s", from);
        // get the date field
        fseek(f, 6, SEEK_CUR);
        char date[11];
        fscanf(f, "%s", date);

        sprintf(list_resp, "%d: Email from %s (%s)\n", id, from, date);
        int l = strlen(list_resp);
        for(int i=l; i<200; i++) {
            list_resp[i] = '\0';
        }
        send(sockfd, list_resp, 200, 0);
    }

    list_resp[0] = '~';
    for(int i=1; i<200; i++) {
        list_resp[i] = '\0';
    }
    send(sockfd, list_resp, 200, 0);
    fclose(f);
    return;
}

int main(int argc, char* argv[]) {
    if(argc != 2) {
        printf("Usage: %s <server-port>\n", argv[0]);
        exit(0);
    }

    MY_SMTP_PORT = atoi(argv[1]);

    int server_sockfd, comm_sockfd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t clilen;

    if((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed\n");
        exit(0);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(MY_SMTP_PORT);
    
    if(bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("local address binding failed\n");
        exit(0);
    }
    
    int flags = fcntl(server_sockfd, F_GETFL, 0);
    fcntl(server_sockfd, F_SETFL, flags|O_NONBLOCK);

    listen(server_sockfd, 5);

    while(1) {
        usleep(1000);

        clilen = sizeof(client_addr);
        comm_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &clilen);

        if(comm_sockfd < 0) {
            if(errno != EWOULDBLOCK && errno != EAGAIN) {
                perror("accept error");
                exit(1);
            }
            // continue because we are in non-blocking mode (EWOULDBLOCK or EAGAIN)
            continue;
        }

        flags = fcntl(comm_sockfd, F_GETFL, 0);
        fcntl(comm_sockfd, F_SETFL, flags|O_NONBLOCK);

        // store client details in server
        char* client_ip = inet_ntoa(client_addr.sin_addr);

        char mail_from[30], rcpt_to[30], client_domain[30], mail[30];
        int mail_id = -1;
        char mail_content[10000];
        bzero(mail_content, 10000);
        bzero(mail_from, 30);
        bzero(rcpt_to, 30);
        bzero(client_domain, 30);
        bzero(mail, 30);

        // char command_content[50], mail_content[10000];

        printf("Client connected: %s", client_ip);

        pid_t pid = fork();
        if(!pid) {
            time_t connection_time = time(NULL);
            int rec_cnt = 0;
            close(server_sockfd);

            while(1) {
                char command[100];
                int temp = 0, state = 0;

                while(1) {
                    char crec;
                    int num_received = recv(comm_sockfd, &crec, 1, 0);

                    if(num_received < 0) {
                        if(errno == EWOULDBLOCK || errno == EAGAIN) {
                            usleep(1000);
                            continue;
                        } 
                        else {
                            perror("receive error");
                            close(comm_sockfd);
                            exit(1);
                        }
                    }
                    else if(num_received == 0) {
                        printf("Client %s disconnected\n", client_ip);
                        close(comm_sockfd);
                        temp = 1;
                    }

                    if(!memcmp(command, "DATA", 4)) {
                        // if(state == 3)
                        state = 0;
                        while(1) {
                            char mc;
                            int num_received = recv(comm_sockfd, &mc, 1, 0);
        
                            if(num_received < 0) {
                                if(errno == EWOULDBLOCK || errno == EAGAIN) {
                                    usleep(1000);
                                    continue;
                                } 
                                else {
                                    perror("receive error");
                                    close(comm_sockfd);
                                    exit(1);
                                }
                            }
                            else if(num_received == 0) {
                                printf("Client %s disconnected\n", client_ip);
                                close(comm_sockfd);
                                temp = 1;
                            } 

                            if(mc == '\n' && state == 0) {
                                state = 1;
                            }
                            else if(mc == '.' && state == 1) {
                                state = 2;
                            } 
                            else if(mc == 'n' && state == 2) {
                                state = 3;
                                break;
                            }
                            else {
                                state = 0;
                                strncat(mail_content, &mc, 1);
                            }
                        }
                        if(state == 3) {
                            break;
                        }
                    }

                    if(crec ==  '\r') break;

                    strncat(command, &crec, 1);
                }

                if(temp) exit(1);

                response resp;
                
                if(!memcmp(command, HELO, 5)) {
                    // assuming domain is correct (not checking .xyz)
                    strncpy(client_domain, command+5, 30);
                    printf("HELO received from %s\n", client_domain);
                    sendResponse(200, "OK", comm_sockfd);
                }
                else if(!memcmp(command, MAIL_FROM, 11)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    strncpy(mail_from, command+11, 30);
                    printf("MAIL FROM: %s\n", mail_from);
                    sendResponse(200, "OK", comm_sockfd);
                }
                else if(!memcmp(command, RCPT_TO, 9)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    strncpy(rcpt_to, command+9, 30);
                    printf("RCPT TO: %s\n", rcpt_to);

                    create_mailbox(client_domain, mail_from, rcpt_to, comm_sockfd);

                    sendResponse(200, "OK", comm_sockfd);
                }
                else if(!memcmp(command, DATA, 5)) {
                    if((strlen(mail_from) == 0) || (strlen(rcpt_to) == 0)) {
                        sendResponse(403, "Sender, Receiver not known", comm_sockfd);
                        continue;
                    }
                    strncpy(mail_content, command+5, 10000);

                    if(store_message(mail_from, rcpt_to, mail_content, &rec_cnt) < 0) {
                        sendResponse(500, "Server Error, could not store message", comm_sockfd);
                        continue;
                    }

                    printf("DATA received, message stored\n");
                    sendResponse(200, "Message stored successfully", comm_sockfd);
                }
                else if(!memcmp(command, LIST, 5)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    strncpy(mail, command+5, 30);
                    printf("LIST %s", mail);
                    sendResponse(200, "OK", comm_sockfd);
                    sendList(mail, comm_sockfd);
                }
                else if(!memcmp(command, GET_MAIL, 9)) {
                    strncpy(mail, command+9, 30);
                }
                else if(!memcmp(command, QUIT, 4)) {
                    sendResponse(200, "Goodbye", comm_sockfd);
                    printf("Client disconnected\n");
                    close(comm_sockfd);
                    exit(0);
                }
                else {
                    sendResponse(403, "Initiate Communication with HELO", comm_sockfd);
                }
            }
        }
    }
    
}