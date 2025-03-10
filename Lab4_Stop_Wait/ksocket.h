#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <stdbool.h>

#define ENOTBOUND 1
#define ENOMESSAGE 2
#define SOCK_KTP 100
#define P 0.1
#define T 5

int error_var;

struct message {
    int type;
    int seq;
    char data[504];
};