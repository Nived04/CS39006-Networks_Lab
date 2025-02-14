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

#define PORT 5051
#define MAXCLI 5

int main() {
    int ssock_fd, clilen;
    struct sockaddr_in server_addr;
    fd_set fds;

    FD_ZERO(&fds);

    int numclient = 0;
    int clientsockfd[MAXCLI];
    
    char client_ips[MAXCLI][100];
    int client_ports[MAXCLI];

    ssock_fd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if(bind(ssock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Could not bind\n");
        exit(0);
    }

    listen(ssock_fd, MAXCLI);
    char buff[1000];

    int numscl = 0;

    while(1) {
        fflush(NULL);

        FD_ZERO(&fds);
        FD_SET(ssock_fd, &fds);
        numscl = ssock_fd;
        
        for(int i=0; i<numclient; i++) {
            FD_SET(clientsockfd[i], &fds);
            numscl = (numscl > clientsockfd[i]) ? numscl : clientsockfd[i];
        }

        select(numscl + 1, &fds, NULL, NULL, NULL); // mistake during lab test: need to give highest fd + 1, not maximun number of fds

        if(FD_ISSET(ssock_fd, &fds)) {
            struct sockaddr_in client_addr;
            clilen = sizeof(client_addr);
            
            int newsockfd = accept(ssock_fd, (struct sockaddr *)&client_addr, &clilen);

            numclient++;
            clientsockfd[numclient - 1] = newsockfd;
            client_ports[numclient - 1] = client_addr.sin_port;
            strcpy(client_ips[numclient - 1], inet_ntoa(client_addr.sin_addr));
            
            printf("Server: Received a new connection from client %s: %d\n", client_ips[numclient - 1], client_ports[numclient - 1]);
            continue;
        }

        for(int i=0; i<numclient; i++) {
            if(FD_ISSET(clientsockfd[i], &fds)) {
                int num_bytes;
                memset(buff, 0, sizeof(buff));
                if((num_bytes = recv(clientsockfd[i], buff, 1000, 0)) < 0) { // mistake during lab test: forget brackets around num_bytes
                    printf("Receive error for %s: %d\n", client_ips[i], client_ports[i]);
                    continue;
                }

                if (num_bytes == 0) {
                    printf("Server: Client %s: %d disconnected at socket %d\n", client_ips[i], client_ports[i], clientsockfd[i]);
                    close(clientsockfd[i]);
                    continue;
                }
                buff[num_bytes] = '\0';
                
                // number of bytes received is 0 always, could not debug why. 
                // printf("TEST:: %d\n", num_bytes); 

                if(numclient < 2) {
                    printf("Server: Insufficient clients, \"%s\" from client %s: %d dropped\n", buff, client_ips[i], client_ports[i]);
                }
                else {
                    printf("Server: Received message \"%s\" from client %s: %d\n", buff, client_ips[i], client_ports[i]);

                    for(int j=0; j<numclient; j++) {
                        if(j!=i) {
                            while(send(clientsockfd[j], buff, strlen(buff) + 1, 0) < 0) {
                                printf("Server: could not relay message, trying again...\n");
                            }
                            printf("Server: Send message \"%s\" from client %s: %d to %s: %d\n", buff, client_ips[i], client_ports[i], client_ips[j], client_ports[j]);
                        }
                    }
                }
            }
        }
    }
}