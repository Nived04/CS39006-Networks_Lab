#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 5050
#define BUFFER 100

char* to_upper(char* str) {
    char* temp = (char*)malloc(26*sizeof(char));
    for(int i=0; i<26; i++) {
        if(str[i] >= 'a' && str[i] <= 'z') {
            temp[i] = str[i] - 'a' + 'A';
        }
        else {
            temp[i] = str[i];
        }
    }
    return temp;
}

void send_message(int client_sockfd, char* message) {
    int message_size = strlen(message) + 1;
    while(message_size > BUFFER) {
        send(client_sockfd, message, BUFFER, 0);
        message_size -= BUFFER;
        message += BUFFER;
    }
    if(message_size > 0) {
        send(client_sockfd, message, message_size, 0);
    }
}

char* get_file_contents(FILE* fp) {
    char* file_content = (char*)malloc(1000*sizeof(char));
    char c; int i=0;
    while((c=fgetc(fp)) != EOF) {
        file_content[i] = c;
        i++;
    }
    file_content[i] = '\0';
    return file_content;
}

int main() {
    int client_sockfd;
    struct sockaddr_in server_addr;

    client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_sockfd < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    server_addr.sin_family  = AF_INET;
    server_addr.sin_port    = htons(20000);
    inet_aton("127.0.0.1", &server_addr.sin_addr);

    if ((connect(client_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
		perror("Unable to connect to server\n");
		exit(0);
	}
    
    char *file_name         = (char*)malloc(100*sizeof(char));
    char *not_found_error   = (char*)malloc(100*sizeof(char));
    FILE* fp;

    while(1) {
        bzero(file_name, 100);
        printf("Enter the file name to encrypt: ");
        scanf("%s", file_name);

        // pre-defining not-found error message to identify when received from server
        sprintf(not_found_error, "NOTFOUND %s", file_name);

        fp = fopen(file_name, "r");
        if(fp == NULL) {
            printf("Error: %s\nPlease Re-try.\n\n", not_found_error);
        }
        else break;
    }

    char key[27];
    int map[26];
    fflush(NULL);
    while(1) {
        for(int i=0; i<26; i++) map[i] = 0;
        printf("Enter the key: ");
        scanf(" %s", key);

        char* temp = (char*)malloc(26*sizeof(char));
        temp = to_upper(key);

        fflush(NULL);

        if(strlen(temp) == 26) {
            int flag = 1;
            for(int i=0; i<26; i++) {
                map[temp[i] - 'A']++;
                if(map[temp[i] - 'A'] > 1) {
                    printf("Key cannot have repeated characters\n");
                    flag = 0;
                    break;
                }
            }
            if(flag) break;
        }
        else {
            printf("Key must be of length 26\n");
        }
    }

    char* file_content = get_file_contents(fp);

    fclose(fp);

    printf("%s\n", file_content);

    send_message(client_sockfd, key);
    send_message(client_sockfd, file_content);


    // char c ='-';
    // char buff[BUFFER];

    // FILE* enc_file = fopen("enc_file.txt", "w");

    // while(1) {
    //     recv(client_sockfd, buff, BUFFER, 0);
    //     int flag = 0;
    //     for(int i=0; i<strlen(buff); i++) {
    //         c = buff[i];
    //         if(c == '%') {
    //             flag = 1;
    //             break;   
    //         }
    //         fprintf(enc_file, "%c", c);
    //         bzero(buff, BUFFER);
    //     }
    //     if(flag == 1) break;
    // }
}