## Header List and their use:
- <sys/socket.h>&emsp;: socket()
- <netinet/in.h>&nbsp;&emsp;: sockaddr_in
- <arpa/inet.h>&nbsp;&nbsp;&emsp;: inet_pton(), inet_ntop()
- <unistd.h>&emsp;&emsp;&emsp;: close(), fork(), read(), write(), pipe()
- <strings.h>&emsp;&emsp;&ensp;&nbsp;: bzero()
- <sys/types.h> &emsp;&nbsp;: ssize_t, pid_t, socklen_t
- <sys/select.h>&emsp;&nbsp;: select()
- <sys/time.h> &emsp;&ensp;&nbsp;: struct timeval
- <fcntl.h>&emsp;&emsp;&emsp;&ensp;: fcntl()
- <errno.h>&emsp;&emsp;&emsp;&nbsp;: errno
- <netdb.h>&emsp;&emsp;&emsp;: getaddrinfo(), getnameinfo()
- <sys/sem.h>&emsp;&emsp;&nbsp;: semop(), semctl(), semget()

## Different types of flags:

MSG_WAITALL: 
```
    // This will wait until the buffer is full of 1024 bytes
    char buff[1024];
    recv(sock, buff, sizeof(buff), MSG_WAITALL);
```

But use of this flag is not recommended as it can lead to deadlock/infinite blocking.

MSG_DONTWAIT:
```
    // This will return immediately if there is no data to read
    char buff[1024];
    recv(sock, buff, sizeof(buff), MSG_DONTWAIT);
```

error_code : EWOULDBLOCK || EAGAIN

Syscall:
The following calls can be used to make the **entire socket operation** non-blocking:
fcntl
ioctl

```
int flag = fcntl(sock, F_GETFL, 0);
fcntl(sock, F_SETFL, flag | O_NONBLOCK);
```

if the setfl is called before, we may lose the flags already set for the sockfd. That is why, 
we first get the flags and then set the flags.

## Errors encountered:
1. bind() returns -1:
- Address already in use:
    Socket goes into a TIME_WAIT state

    Possible reason: server closed before client, and the port is still in use. 

    Solution: Use SO_REUSEADDR socket option.  
    `int optval = 1;`  
    `setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));`

# Raw Sockets:
- Requires root privileges. They bypass the transport layer and directly send packets.

## Function workings:
- **inet_ntop(int af, const void\* src, char\* dst, socklen_t size)**:  
    fill size bytes of dst with the string representation of src (struct in_addr). If size is greater, fills remaining bytes with \0 

## Some Nooks and Cranies to take care of:
- When using fork(), then although the child process gets the entire copy of parent's memory, any change to parent's memory will not be 
reflected. Where is this used? We may use the same sockfd in child and parent and it will work. But by mistake, if the parent or the child 
changes some socket property (such as making it non-blocking), the other process will not know of this, hence may cause problems. But for 
normal operations, child can use the same sockfd as parent, without creating a new socket.
 
- if using MSG_WAITALL, make sure to handle variable data lengths in case required. If we use strlen for data bytes, it will capture length till 
first \0. and if the property is set to MSG_WAITALL for recv, then its possible that the data is clubbed with the next data. Hence, either use memcpy
instead of string cpy, or use strncpy, or manually add extra null bytes. 