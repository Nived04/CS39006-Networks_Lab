/*
Assignment 7 Submission
Name: Nived Roshan Shah
Roll No: 22CS10049
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <strings.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <errno.h>

#define PROTO_CLDP      253
#define BUFFER_SIZE     2048
#define QUERY_INTERVAL  20

#define TYPE_HELLO      1
#define TYPE_QUERY      2
#define TYPE_RESPONSE   3

typedef struct {
    unsigned int type;
    unsigned int payload_length;
    unsigned int transaction_id;
    unsigned int reserved;
} cldp_headers;

typedef struct node_list {
    time_t active_time;
    int expected_tid;
    char node_ip[50];
    struct node_list* next;
} node_list;

int tid = 0;

// Add a node to the linked list
void add_node(char* ip, node_list** head) {
    node_list* current = *head;
    while(current != NULL) {
        if(strcmp(current->node_ip, ip) == 0) {
            // update active time because fresh "HELLO" was received
            current->active_time = time(NULL);
            // current->expected_tid = ++tid;
            return;
        }
        current = current->next;
    }
    
    node_list* new_node = (node_list*)malloc(sizeof(node_list));
    
    strcpy(new_node->node_ip, ip);
    new_node->active_time = time(NULL);
    // new_node->expected_tid = ++tid;
    new_node->next = NULL;

    if(*head == NULL) {
        *head = new_node;
        return;
    }
    
    current = *head;
    while(current->next != NULL) {
        current = current->next;
    }
    current->next = new_node;
    
    return;
}

// Delete a node from the linked list
void delete_node(char* search_ip, node_list** head) {
    if(*head == NULL) {
        return;
    }

    node_list* current = *head;
    node_list* prev = NULL;
    
    while(current != NULL) {
        if(strcmp(current->node_ip, search_ip) == 0) {
            if(prev == NULL) {
                *head = current->next;
            }
            else {
                prev->next = current->next;
            }
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
    return;
}

// Check for inactive nodes (nodes that haven't sent HELLO in more than 30 seconds)
void check_inactive_nodes(node_list** head) {
    if(*head == NULL) {
        return;
    }
    
    time_t current_time = time(NULL);
    node_list* current = *head;
    node_list* prev = NULL;
    
    while(current != NULL) {
        if(current_time - current->active_time > 30) {
            printf("Node %s has not sent HELLO for over 30 seconds: removing from list\n", current->node_ip);
            
            if(prev == NULL) {
                *head = current->next;
                free(current);
                current = *head;
            } 
            else {
                prev->next = current->next;
                node_list* to_delete = current;
                current = current->next;
                free(to_delete);
            }
        } 
        else {
            prev = current;
            current = current->next;
        }
    }
}

// Get the non-loopback IP address of local machine
in_addr_t get_local_ip() {
    char ip_str[INET_ADDRSTRLEN];
    struct ifaddrs *ifaddr, *ifa;
    in_addr_t ip_addr = INADDR_NONE;
    
    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs failed");
        return INADDR_NONE;
    }
    
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        
        struct sockaddr_in *addr = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ip_str, sizeof(ip_str));
        
        if (strncmp(ip_str, "127.", 4) != 0) {
            ip_addr = addr->sin_addr.s_addr;  
            break;
        }
    }
    
    freeifaddrs(ifaddr);
    return ip_addr;
}

// Compute IP checksum
unsigned short compute_checksum(unsigned short *buf, int nwords) {
    unsigned long sum = 0;
    for (int i = 0; i < nwords; i++) {
        sum += buf[i];
    }
    while(sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return (unsigned short)(~sum);
}

// send query to the destination ip
void send_query(int sock, char* dest_ip) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    
    struct iphdr *ip = (struct iphdr *)buffer;
    cldp_headers *cldp = (cldp_headers *)(buffer + sizeof(struct iphdr));
    
    cldp->type = TYPE_QUERY;
    cldp->payload_length = 0;
    cldp->transaction_id = htons(++tid);
    cldp->reserved = 0;
    
    ip->version = 4;
    ip->ihl = 5;
    ip->tos = 0;
    ip->tot_len = htons(sizeof(struct iphdr) + sizeof(cldp_headers));
    ip->id = htons(rand() % 65535);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = PROTO_CLDP;
    ip->saddr = get_local_ip();
    ip->daddr = inet_addr(dest_ip);
    ip->check = compute_checksum((unsigned short *)ip, ip->ihl * 2);
    
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(0);
    dest.sin_addr.s_addr = ip->daddr;

    if (sendto(sock, buffer, ntohs(ip->tot_len), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
        perror("Sendto QUERY failed");
    } 
    else {
        printf("Sent QUERY to %s\n", dest_ip);
    }
}

// iterate through the list of active server ips and send a query to them
void query_all_nodes(int sock, node_list* head) {
    node_list* current = head;
    while(current != NULL) {
        send_query(sock, current->node_ip);
        current->expected_tid = tid;
        current = current->next;
    }
}

int check_tid(char *ip, int tid, node_list* head) {
    node_list* current = head;
    while(current != NULL) {
        if(strcmp(current->node_ip, ip) == 0) {
            if(current->expected_tid == tid) {
                return 1; 
            } else {
                printf("Invalid transaction ID from %s: expected %d, got %d\n", ip, current->expected_tid, tid);
                return 0;
            }
        }
        current = current->next;
    }
    return 0;
}

void print_nodes(node_list* head) {
    printf("\n--- Active Nodes ---\n");
    if(head == NULL) {
        printf("No active nodes\n\n");
        return;
    }
    
    node_list* current = head;
    int count = 0;
    while(current != NULL) {
        printf("%d. %s\n", ++count, current->node_ip);
        current = current->next;
    }
    printf("-------------------\n\n");
}

int main() {
    int sock = socket(AF_INET, SOCK_RAW, PROTO_CLDP);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    int one = 1;
    if (setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        perror("setsockopt IP_HDRINCL failed");
        exit(1);
    }

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    srand(time(NULL));
    
    node_list* head = NULL;
    
    time_t last_query_time = 0;
    time_t last_check_time = 0;
    
    char buffer[BUFFER_SIZE];
    
    printf("CLDP Client started. Listening for HELLO messages...\n");
    
    while(1) {
        time_t current_time = time(NULL);
        
        if(current_time - last_check_time > QUERY_INTERVAL) {
            check_inactive_nodes(&head);
            last_check_time = current_time;
        }
        
        if(current_time - last_query_time > QUERY_INTERVAL) {
            printf("Querying all active nodes...\n");
            query_all_nodes(sock, head);
            print_nodes(head);
            last_query_time = current_time;
        }
        
        bzero(buffer, BUFFER_SIZE);
        struct sockaddr_in src_addr;
        socklen_t addr_len = sizeof(src_addr);
        
        int n = recvfrom(sock, buffer, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *)&src_addr, &addr_len);
        
        if (n < 0) {
            if(errno != EAGAIN && errno != EWOULDBLOCK) {
                perror("recvfrom failed");
                exit(1);
            }

            usleep(100000);
            continue;
        }

        struct iphdr *ip = (struct iphdr *)buffer;

        // following filter not necessary because socket is defined with PROTO_CLDP protocol
        if(ip->protocol != PROTO_CLDP) {
            continue;
        }

        cldp_headers *cldp = (cldp_headers *)(buffer + ip->ihl*4);
        char src_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &ip->saddr, src_ip, sizeof(src_ip));

        switch(cldp->type) {
            case TYPE_HELLO:
                printf("Received HELLO from %s\n", src_ip);
                add_node(src_ip, &head);
                break;  
                
            case TYPE_RESPONSE:
                {
                    if(check_tid(src_ip, ntohs(cldp->transaction_id), head)) {
                        char *payload = NULL;
                        if(cldp->payload_length > 0) {
                            payload = buffer + ip->ihl*4 + sizeof(cldp_headers);
                            printf("Received RESPONSE from %s:\n%s\n\n", src_ip, payload);
                        }
                    }
                    else {
                        printf("Transaction not matched, received TID: %d\n\n", ntohs(cldp->transaction_id));
                    }
                }
                break;
        }
    }

    close(sock);
    return 0;
}