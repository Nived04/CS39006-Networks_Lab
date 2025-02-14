/*
Set: A
Assumptions: server is always on (not abruptly closed, eg. using ctrl+C), same for client
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/select.h>

#define STDIN 0
#define PORT 5051

int main() {
    fd_set fds;
    int sockfd;
    struct sockaddr_in server_addr;

    while((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Client: Error in creating socket, trying again...\n");
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int maxfd = (sockfd > STDIN) ? sockfd : STDIN;

    while(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ) {
        printf("Client: Error in connecting to server, trying again...\n");
        sleep(5);
    }
    

    while(1) {
        FD_ZERO(&fds);
        FD_SET(sockfd, &fds);
        FD_SET(STDIN, &fds);

        char user_input[1000], buff[1000];
        int nread;

        select(maxfd + 1, &fds, NULL, NULL, NULL);
        if (FD_ISSET(STDIN, &fds)){
            memset(user_input, 0, 1000);
			nread = read(0, user_input, 1000);
			user_input[nread-1]='\0';
        
            int temp = 0;

            while((temp=send(sockfd, user_input, strlen(user_input) + 1, 0)) < 0) {
                printf("Client: Error in sending message, trying again...\n");
            }
            // printf("Test: %d\n", temp); temp is printing correct value of bytes, 
            // but server is not always receiving 0 bytes for some reason
            printf("Client: Message \"%s\" sent to server\n", user_input);

		}

		if (FD_ISSET(sockfd, &fds)){
            nread = recv(sockfd, buff, 1000, 0);

            if (nread == 0) {
                printf("Client: Server closed connection\n");
                break;
            }

            // struct sockaddr_in peer;
            // getpeername(sockfd, (struct sockaddr *)&peer, sizeof(peer));

            // int port = ntohs(peer.sin_port);
            // printf("%d\n", port);

            printf("Client: Received Message \"%s\"\n", buff);
		}
    }
}