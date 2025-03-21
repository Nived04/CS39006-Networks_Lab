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
#include <sys/stat.h>

#define HELO "HELO "
#define MAIL_FROM "MAIL FROM: "
#define RCPT_TO "RCPT TO: "
#define DATA "DATA"
#define LIST "LIST "
#define GET_MAIL "GET_MAIL "
#define QUIT "QUIT"

int MY_SMTP_PORT;

// response sent by the server
typedef struct {
    int status;
    char response_message[50];
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
    strncpy(resp.response_message, message, sizeof(resp.response_message) - 1);
    resp.response_message[sizeof(resp.response_message) - 1] = '\0';

    send(sockfd, &resp, sizeof(response), 0);
}

// check for domain format: x.y
int checkDomain(char domain[]) {
    int len = strlen(domain);

    if (len < 3) {
        return -1;
    }
    
    int found_period = 0;
    for (int i = 1; i < len - 1; i++) {
        if (domain[i] == '.' && 
            domain[i-1] != ' ' && domain[i-1] != '\0' && 
            domain[i+1] != ' ' && domain[i+1] != '\0') {
            found_period = 1;
            break;
        }
    }
    
    return found_period ? 0 : -1;
}

// mail format must be x@y.z
int checkMail(char mail[]) {
    int len = strlen(mail);
    
    if (len < 5) {
        return -1;
    }
    
    int at_pos = -1;
    for (int i = 0; i < len; i++) {
        if (mail[i] == '@') {
            at_pos = i;
            break;
        }
    }
    
    if (at_pos <= 0 || at_pos >= len - 3) {
        return -1;
    }
    
    int dot_found = 0;
    for (int i = at_pos + 2; i < len - 1; i++) {
        if (mail[i] == '.') {
            dot_found = 1;
            break;
        }
    }
    
    if (!dot_found) {
        return -1;
    }
    
    return 0;
}

int checkMailDomain(char mail[], char domain[]) {
    int flag = 0;
    for(int i=0, j=0; i<strlen(mail); i++) {
        if(mail[i] == '@') {
            flag = 1;
            continue;
        }
        if(flag) {
            if(mail[i] != domain[j])
                return -1;
            j++;
        }
    }
    if(!flag) {
        return -1;
    }
    return 0;
}

// create a file in the mailbox folder, for the recipient
void create_mailbox(char domain[], char from[], char to[], int fd) {
    mkdir("mailbox", 0777);
    char filename[30];
    sprintf(filename, "./mailbox/%s.txt", to);
    FILE* f = fopen(filename, "a+");
    if(f == NULL) {
        perror("Error creating mailbox");
    }
    fclose(f);
    return;
}

// store the received mail into the recipient's mailbox
int store_message(char from[], char to[], char content[]) {
    char date[30];
    time_t now = time(NULL);
    struct tm *current_dt = localtime(&now);
    
    snprintf(date, sizeof(date), "%02d-%02d-%4d", 
             current_dt->tm_mday, 
             current_dt->tm_mon+1, 
             current_dt->tm_year+1900);
    
    date[sizeof(date) - 1] = '\0';
    date[10] = '\0';
    char filename[30];
    sprintf(filename, "./mailbox/%s.txt", to);
    
    int next_id = 1;
    FILE* f_read = fopen(filename, "r");
    if (f_read != NULL) {
        char line[100];
        int max_id = 0;
        
        while (fgets(line, sizeof(line), f_read) != NULL) {
            if (strncmp(line, "Mail_ID: ", 9) == 0) {
                int current_id = 0;
                sscanf(line + 9, "%d", &current_id);
                if (current_id > max_id) {
                    max_id = current_id;
                }
            }
        }
        
        next_id = max_id + 1;
        fclose(f_read);
    }
    
    FILE* f = fopen(filename, "a+");
    if(f == NULL) {
        perror("Error opening file");
        return -1;
    }

    // since content ends with a . on a new line, we remove the last two characters
    int l = strlen(content);
    if(l>=2) {
        content[l-2] = '\0';
        content[l-1] = '\0';
    }
    
    if(fprintf(f, "-----\nMail_ID: %d\nFrom: %s\nDate: %s\n%s\n", next_id, from, date, content) < 0) {
        perror("Error writing to file");
        fclose(f);
        return -1;
    }
    
    fclose(f);
    return 0;
}

// send the list of emails in the recipient's mailbox
int sendList(char mail[], int sockfd) {
    char filename[30];
    sprintf(filename, "./mailbox/%s.txt", mail);
    FILE* f = fopen(filename, "r");

    if(f == NULL) {
        sendResponse(401, "NOT FOUND", sockfd);
        return -1;
    }
    
    sendResponse(200, "OK", sockfd);
    
    char list_resp[200];
    char line[100];
    
    while(1) {  
        bzero(list_resp, 200);
        int found_marker = 0;
        
        while(!found_marker && fgets(line, sizeof(line), f) != NULL) {
            if(strncmp(line, "-----", 5) == 0) {
                found_marker = 1;
            }
        }
        
        if(!found_marker) break;
        
        if(fgets(line, sizeof(line), f) == NULL) break;
        int id = 0;
        sscanf(line, "Mail_ID: %d", &id);
        
        if(fgets(line, sizeof(line), f) == NULL) break;
        char from[30] = {0};
        sscanf(line, "From: %s", from);
        
        if(fgets(line, sizeof(line), f) == NULL) break;
        char date[30] = {0};
        sscanf(line, "Date: %s", date);
        
        sprintf(list_resp, "%d: Email from %s (%s)\n", id, from, date);
        
        int l = strlen(list_resp);
        for(int i=l; i<200; i++) {
            list_resp[i] = '\0';
        }

        send(sockfd, list_resp, 200, 0);
    }

    // Send terminator
    list_resp[0] = '~';
    for(int i=1; i<200; i++) {
        list_resp[i] = '\0';
    }
    send(sockfd, list_resp, 200, 0);

    fclose(f);
    return 0;
}

int sendMail(char mail[], int id, int sockfd) {
    char filename[30];
    sprintf(filename, "./mailbox/%s.txt", mail);
    FILE* f = fopen(filename, "r");

    if(f == NULL) {
        sendResponse(401, "NOT FOUND", sockfd);
        return -1;
    }

    int found = 0;
    char mail_content[10000] = "";
    char line[1000];
    while(1) {
        if(fgets(line, 1000, f) == NULL) {
            break;
        }
        // get the new mail using the demarcation -----
        if(strstr(line, "-----") != NULL) {
            if(fgets(line, 1000, f) == NULL) break;
            
            char id_str[15];
            sprintf(id_str, "Mail_ID: %d", id);

            if(strstr(line, id_str) != NULL) {
                found = 1;
                if(fgets(line, 1000, f) != NULL) strcat(mail_content, line);
                if(fgets(line, 1000, f) != NULL) strcat(mail_content, line);
                
                while(fgets(line, 1000, f) != NULL) {
                    if(strstr(line, "-----") != NULL)
                        break;
                    strcat(mail_content, line);
                }
                break;
            }
        }
    }

    if(found) {
        sendResponse(200, "OK", sockfd);
        send(sockfd, mail_content, strlen(mail_content), 0);
    } else {
        sendResponse(401, "NOT FOUND", sockfd);
        fclose(f);
        return -1;
    }

    fclose(f);

    return 0;
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
    memset(server_addr.sin_zero, '\0', sizeof(server_addr.sin_zero));

    if(bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("local address binding failed\n");
        exit(0);
    }
    
    int flags = fcntl(server_sockfd, F_GETFL, 0);
    fcntl(server_sockfd, F_SETFL, flags|O_NONBLOCK);

    listen(server_sockfd, 5);
    
    printf("Listening on port %d...\n", MY_SMTP_PORT);

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

        printf("Client connected: %s\n", client_ip);
        
        pid_t pid = fork();
        if(!pid) {
            // variables to store mail details
            char mail_from[30], rcpt_to[30], client_domain[30], mail[30];
            int mail_id = -1;
            char mail_content[10000];

            close(server_sockfd);
            
            bzero(mail_content, 10000);
            bzero(mail_from, 30);
            bzero(rcpt_to, 30);
            bzero(client_domain, 30);
            bzero(mail, 30);

            while(1) {
                char command[100];
                bzero(command, 100);
                int state = 0;

                while(1) {
                    int num_received = recv(comm_sockfd, command, 100, 0);

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
                        exit(1);
                    }

                    break;
                }

                if(!memcmp(command, HELO, 5)) {
                    strncpy(client_domain, command+5, 30);
                    client_domain[strlen(client_domain)-1] = '\0';
                    
                    if(checkDomain(client_domain) < 0) {
                        sendResponse(403, "Invalid domain", comm_sockfd);
                        continue;
                    }

                    printf("HELO received from %s\n", client_domain);
                    sendResponse(200, "OK", comm_sockfd);
                }
                else if(!memcmp(command, MAIL_FROM, 11)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    strncpy(mail_from, command+11, 30);
                    mail_from[strlen(mail_from)-1] = '\0';

                    if(checkMail(mail_from) < 0) {
                        sendResponse(403, "Invalid email", comm_sockfd);
                        continue;
                    }

                    printf("MAIL FROM: %s\n", mail_from);

                    if(checkMailDomain(mail_from, client_domain) < 0) {
                        sendResponse(403, "Invalid domain", comm_sockfd);
                        continue;
                    }

                    sendResponse(200, "OK", comm_sockfd);
                }
                else if(!memcmp(command, RCPT_TO, 9)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    strncpy(rcpt_to, command+9, 30);
                    rcpt_to[strlen(rcpt_to)-1] = '\0';

                    if(checkMail(rcpt_to) < 0) {
                        sendResponse(403, "Invalid email", comm_sockfd);
                        continue;
                    }

                    printf("RCPT TO: %s\n", rcpt_to);

                    create_mailbox(client_domain, mail_from, rcpt_to, comm_sockfd);

                    sendResponse(200, "OK", comm_sockfd);
                }
                else if(!memcmp(command, DATA, 4)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    if((strlen(mail_from) == 0) || (strlen(rcpt_to) == 0)) {
                        sendResponse(403, "Sender or Receiver not known", comm_sockfd);
                        continue;
                    }

                    sendResponse(200, "Proceed to enter message", comm_sockfd);

                    state = 0;
                    while(1) {
                        // read the mail content character by character
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
                            exit(1);
                        } 
                        
                        // an FSM logic to match the \n.\n ending of the mail
                        if(mc == '\n' && state == 0) {
                            state = 1;
                        }
                        else if(mc == '.' && state == 1) {
                            state = 2;
                        } 
                        else if(mc == '\n' && state == 2) {
                            state = 3;
                            break;
                        }
                        else {
                            state = 0;
                        }
                        strncat(mail_content, &mc, 1);
                    }

                    if(store_message(mail_from, rcpt_to, mail_content) < 0) {
                        sendResponse(500, "Server Error, could not store message", comm_sockfd);
                        continue;
                    }

                    printf("DATA received, message stored.\n");
                    sendResponse(200, "Message stored successfully", comm_sockfd);
                    
                    bzero(mail_content, 10000);
                    bzero(mail_from, 30);
                    bzero(rcpt_to, 30);
                }
                else if(!memcmp(command, LIST, 5)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    strncpy(mail, command+5, 30);
                    mail[strlen(mail)-1] = '\0';

                    printf("LIST %s\n", mail);
                    sendList(mail, comm_sockfd);
                    printf("Emails retrieved; list sent.\n");
                }
                else if(!memcmp(command, GET_MAIL, 9)) {
                    if(strlen(client_domain) == 0) {
                        sendResponse(403, "Initiate communication with HELO", comm_sockfd);
                        continue;
                    }
                    
                    char *email_part = strtok(command+9, " ");
                    if(email_part == NULL) {
                        sendResponse(400, "ERR", comm_sockfd);
                        continue;
                    }
                    
                    strncpy(mail, email_part, 30);
                    
                    char *id_part = strtok(NULL, " ");
                    if(id_part == NULL) {
                        sendResponse(400, "ERR", comm_sockfd);
                        continue;
                    }
                    
                    mail_id = atoi(id_part);
                    printf("GET_MAIL %s %d\n", mail, mail_id);
                    if(sendMail(mail, mail_id, comm_sockfd) < 0) {
                        continue;
                    }
                    printf("Email with id %d sent.\n", mail_id);
                }
                else if(!memcmp(command, QUIT, 4)) {
                    sendResponse(200, "Goodbye", comm_sockfd);
                    printf("Client disconnected.\n");
                    close(comm_sockfd);
                    exit(0);
                }
                else {
                    sendResponse(401, "Invalid Command", comm_sockfd);
                }
            }
        }
        else {
            close(comm_sockfd);
        }
    }
    
    return 0;
}