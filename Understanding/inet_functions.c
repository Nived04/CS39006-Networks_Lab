#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h> 
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main() {
    char ct[INET_ADDRSTRLEN+10];
    char* c = ct;
    char* p = ct;

    for(int i = 0; i < INET_ADDRSTRLEN + 10; i++) {
        printf("%d ", c[i]);
    }

    printf("\n");

    c = "10.5.18.70";
    in_addr_t temp = inet_addr(c);
    
    if (temp == INADDR_NONE) {
        printf("Invalid address\n");
        return 1;
    }

    struct in_addr addr;
    inet_pton(AF_INET, "10.5.18.69", &addr);
    
    c = inet_ntoa(addr);
    printf("%s\n", c); 

    inet_ntop(AF_INET, &addr, p, INET_ADDRSTRLEN-5); // works upto -5

    for(int i = 0; i < INET_ADDRSTRLEN + 10; i++) {
        printf("%d ", p[i]);
    }
    printf("\n");

    printf("%s\n", p);

    char* try;
    int ret = gethostname(try, 10);
    
    printf("%d\n", ret);
}