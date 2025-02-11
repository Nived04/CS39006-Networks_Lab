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

#define BUFFER 100
#define CHUNK_SIZE 20

// function to convert a string to uppercase
char* to_upper(char* str) {
    int n = strlen(str);
    char* temp = (char*)malloc(n*sizeof(char));

    for(int i=0; i<n; i++) 
         temp[i] = (str[i] >= 'a' && str[i] <= 'z') ? str[i] - 'a' + 'A' : str[i];

    return temp;
}

void send_key(int client_sockfd, char* message) {
    int message_size = strlen(message) + 1;

    while(message_size > CHUNK_SIZE) {
        send(client_sockfd, message, CHUNK_SIZE, 0);
        message_size -= CHUNK_SIZE;
        message += CHUNK_SIZE;
    }

    if(message_size > 0) {
        send(client_sockfd, message, message_size, 0);
    }
}

void send_in_chunks(int* comm_sockfd, FILE* fp) {
    char c; int i=0;
    char* file_content = (char*)malloc((CHUNK_SIZE)*sizeof(char));

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
    int client_sockfd;
    struct sockaddr_in server_addr;

    client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_sockfd < 0) {
        perror("Unable to create socket\n");
        exit(0);
    }

    server_addr.sin_family  = AF_INET;
    server_addr.sin_port    = htons(5050);
    inet_aton("127.0.0.1", &server_addr.sin_addr); //inet_aton is obsolete, use inet_pton (compatible with IPv6) 

    if ((connect(client_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr))) < 0) {
		perror("Unable to connect to server\n");
		exit(0);
	}
    
    printf("Connected to server\n");

    char *file_name         = (char*)malloc(100*sizeof(char));
    char *not_found_error   = (char*)malloc(100*sizeof(char));
    FILE* fp;


    while(1) {
        // Get the file name (until a valid file is entered)
        while(1) {
            bzero(file_name, 100);
            printf("Enter the file name to encrypt: ");
            scanf(" %s", file_name);

            // pre-defining not-found error message to identify when received from server
            sprintf(not_found_error, "NOTFOUND %s", file_name);

            fp = fopen(file_name, "r");
            if(fp == NULL) {
                printf("Error: %s\nPlease Re-try.\n", not_found_error);
            }
            else break;
        }

        // array to store the null terminated key
        char key[27];
        int map[26];
        fflush(NULL); 

        // Get the key (until a valid key is entered)
        while(1) {
            // map stores the frequency of each letter in the key
            // required because key cannot have repeated characters
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
                    // if a character is repeated, key is invalid
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

        // store the contents of the file in a string


        // send the key and file content to the server
        send_key(client_sockfd, key);
        send_in_chunks(&client_sockfd, fp);
        
        fclose(fp);

        char buff[BUFFER];

        char* enc_file_name = (char*)malloc(100*sizeof(char));
        sprintf(enc_file_name, "%s.enc", file_name);

        FILE* enc_file = fopen(enc_file_name, "w");

        // receive until the null character is received (means end of file)
        while(1) {
            int num_rec = recv(client_sockfd, buff, BUFFER, 0);

            // printf("--%d\n", num_rec);
            int flag = 0;
            for(int i=0; i<num_rec; i++) {
                // if null char received, set flag to 1 and break
                if(buff[i] == '\0') {
                    flag = 1;
                    break;
                } 
                fputc(buff[i], enc_file);
            }
            // if flag is 1, eof reached, stop receiving
            if(flag) break;
        }

        fclose(enc_file);

        printf("Plaintext file: %s\nCipertext file: %s\n", file_name, enc_file_name);

        char choice[4];
        printf("Do you want to encrypt another file? (Yes/No): ");
        scanf(" %s", choice);

        char msg;
        if(strcmp(choice, "No") == 0) {
            msg = 'N';
            send(client_sockfd, &msg, 1, 0);
            close(client_sockfd);
            break;
        }
        else {
            msg = 'Y';
            send(client_sockfd, &msg, 1, 0);
        }
    }
}