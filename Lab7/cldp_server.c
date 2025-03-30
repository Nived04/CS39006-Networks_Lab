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
#include <ifaddrs.h>

#define PROTO_CLDP          253
#define BUFFER_SIZE         2048
#define MAX_PAYLOAD_SIZE    512
#define HELLO_PERIOD        10
#define CLDP_HEADER_SIZE    16

#define TYPE_HELLO      1
#define TYPE_QUERY      2
#define TYPE_RESPONSE   3

typedef struct {
    unsigned int type;
    unsigned int payload_length;
    unsigned int transaction_id;
    unsigned int reserved;
}cldp_headers;

char cldp_response_payload[MAX_PAYLOAD_SIZE];

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

void get_metadata() {
    struct timeval tv;
    struct tm *tm_info;
    char time_string[50];
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    strftime(time_string, sizeof(time_string), "%Y-%m-%d %H:%M:%S", tm_info);
    
    char hostname[50];
    gethostname(hostname, sizeof(hostname));

    char freeram[50], cpu_load[50];
    struct sysinfo sys_info;
    sysinfo(&sys_info);

    snprintf(freeram, sizeof(freeram), "%lu MB", sys_info.freeram / (1024 * 1024));
    snprintf(cpu_load, sizeof(cpu_load), "%.2f%%", (float)sys_info.loads[0] / 65536.0);

    snprintf(cldp_response_payload, MAX_PAYLOAD_SIZE, 
            "Time: %s | Host: %s | Free RAM: %s | CPU Load: %s",
             time_string, hostname, freeram, cpu_load
    );

    return;
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

    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one)) < 0) {
        perror("setsockopt SO_BROADCAST failed");
        exit(1);
    }
 
    pid_t pid = fork();
    if(!pid) {
        
        char buffer[BUFFER_SIZE];
        while(1) {
            bzero(buffer, BUFFER_SIZE);

            struct iphdr *ip = (struct iphdr *)buffer;

            cldp_headers *cldp = (cldp_headers *)(buffer + sizeof(struct iphdr));

            cldp->type = TYPE_HELLO;
            cldp->payload_length = 0;
            cldp->transaction_id = htons(0);
            cldp->reserved = 0;

            ip->version = 4;
            ip->ihl = 5;
            ip->tos = 0;
            ip->tot_len = htons(sizeof(struct iphdr) + sizeof(cldp_headers));
            ip->id = htons(rand() % 65535);
            ip->frag_off = 0;
            ip->ttl = 64;
            ip->protocol = PROTO_CLDP;
            ip->check = compute_checksum((unsigned short *)ip, ip->ihl * 2);
            ip->saddr = get_local_ip();
            ip->daddr = inet_addr("255.255.255.255");

            struct sockaddr_in dest;
            bzero(&dest, sizeof(dest));
            dest.sin_family = AF_INET;
            dest.sin_port = htons(0);
            dest.sin_addr.s_addr = ip->daddr;

            if (sendto(sock, buffer, ntohs(ip->tot_len), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
                perror("Sendto HELLO failed");
            } else {
                printf("Sent HELLO announcement\n");
            }

            sleep(HELLO_PERIOD);
        }
    }
    else {
        char buffer[BUFFER_SIZE];

        while(1) {
            bzero(buffer, BUFFER_SIZE);
            struct sockaddr_in src_addr;
            socklen_t addr_len = sizeof(src_addr);
            int n = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&src_addr, &addr_len);
            if (n < 0) {
                perror("recvfrom failed");
                continue;
            }

            struct iphdr *ip = (struct iphdr *)buffer;
            if(ip->protocol != PROTO_CLDP) {
                continue;
            }
            
            int min_expected_bytes = ip->ihl*4 + sizeof(cldp_headers);
            if(n < min_expected_bytes) {
                continue;
            }

            cldp_headers *cldp = (cldp_headers *)(buffer + ip->ihl*4);

            if(cldp->type == TYPE_QUERY) {
                char response_payload[MAX_PAYLOAD_SIZE];
                get_metadata();
                strcpy(response_payload, cldp_response_payload);
                int payload_len = strlen(response_payload);

                struct iphdr *ip_response = (struct iphdr *)buffer;
                cldp_headers *cldp_response = (cldp_headers *)(buffer + sizeof(struct iphdr));

                cldp_response->type = TYPE_RESPONSE;
                cldp_response->payload_length = htons(payload_len);
                cldp_response->transaction_id = cldp->transaction_id;
                cldp_response->reserved = 0;

                memcpy(buffer + sizeof(struct iphdr) + sizeof(cldp_headers), response_payload, payload_len);

                ip_response->version = 4;
                ip_response->ihl = 5;
                ip_response->tos = 0;
                ip_response->tot_len = htons(sizeof(struct iphdr) + sizeof(cldp_headers) + payload_len);
                ip_response->id = htons(rand() % 65535);
                ip_response->frag_off = 0;
                ip_response->ttl = 64;
                ip_response->protocol = PROTO_CLDP;
                ip_response->saddr = get_local_ip();
                ip_response->daddr = ip->saddr;
                ip_response->check = compute_checksum((unsigned short *)ip_response, ip_response->ihl * 2);

                struct sockaddr_in dest;
                bzero(&dest, sizeof(dest));
                dest.sin_family = AF_INET;
                dest.sin_port = htons(0);
                dest.sin_addr.s_addr = ip_response->daddr;
                
                if (sendto(sock, buffer, ntohs(ip_response->tot_len), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0) {
                    perror("Sendto RESPONSE failed");
                } else {
                    printf("Sent RESPONSE to %s\n", inet_ntoa(src_addr.sin_addr));
                }
            }
        }
    }

    close(sock);
    return 0;
}