#include "ksocket.h"

struct peer_address {
    char ip[16];
    int port;
};

struct peer_address peer;

int dropMessage(float p) {
    float random = (float)rand() / RAND_MAX;
    return (random < p) ? 1 : 0;
}

int k_socket(int domain, int type, int protocol) {
    if(type == SOCK_KTP)
        return socket(domain, type, protocol);
}

int k_bind(int sockfd, char* source_ip, int source_port, char* dest_ip, int dest_port) {
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));

    addr.sin_addr.s_addr = inet_addr(source_ip);
    addr.sin_port = htons(source_port);
    addr.sin_family = AF_INET;

    int result = bind(sockfd, (struct sockaddr*)&addr, sizeof(addr));
    if(result == 0) {
        strcpy(peer.ip, dest_ip);
        peer.port = dest_port;
    }

    return result;
}

ssize_t k_sendto(int sockfd, const void* buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) {
    struct sockaddr_in addr = *((struct sockaddr_in*)dest_addr);
    
    if(addr.sin_addr.s_addr == inet_addr(peer.ip) && addr.sin_port == htons(peer.port)) {
        return sendto(sockfd, buf, len, flags, dest_addr, addrlen);
    }
    else {
        error_var = ENOTBOUND;
        return -1;
    }
}

ssize_t k_recvfrom(int sockfd, void* buf, size_t len, int flags, struct sockaddr * src_addr, socklen_t * addrlen) {
    
    struct sockaddr_in sender_addr;
    int sender_len = sizeof(sender_addr);

    fd_set readfds;

    while(1) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        struct message* msg = ((struct message*)buf);

        struct timeval timeout;
        timeout.tv_sec = 20;
        timeout.tv_usec = 0;

        int ready = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if(ready == -1) {
            perror("Error in select");
            close(sockfd);
            return 1;
        }
        else if(ready == 0) {
            error_var = ENOMESSAGE;
            printf("No messages received for 20 seconds, returning\n");
            return -1;
        }

        if(FD_ISSET(sockfd, &readfds)) {
            int recv_bytes = recvfrom(sockfd, msg, sizeof(struct message), 0, (struct sockaddr*)&sender_addr, &sender_len);

            if(recv_bytes == -1) {
                perror("Error in recvfrom");
                close(sockfd);
                return 1;
            }

            if(msg->type == 1 && !dropMessage(P)) {
                printf("Received data:\n\n%s\n\n", msg->data);
                
                struct message ack;
                ack.type = 0;
                ack.seq = msg->seq;
    
                int sent_bytes = sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr*)&sender_addr, sender_len);
    
                if(sent_bytes == -1) {
                    perror("Error in sendto");
                    close(sockfd);
                    return 1;
                }
                
                printf("Sent ACK for sequence number: %d\n", ack.seq);
            }
        }
    }

}

int k_close(int fd) {
    return close(fd);
}

