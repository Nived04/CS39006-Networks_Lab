## Different types of flags:

MSG_WAITALL: 
```
    char buff[1024];
    recv(sock, buff, sizeof(buff), MSG_WAITALL);
    // This will wait until the buffer is full of 1024 bytes
```

But use of this flag is not recommended as it can lead to deadlock/infinite blocking.

MSG_DONTWAIT:
```
    char buff[1024];
    recv(sock, buff, sizeof(buff), MSG_DONTWAIT);
    // This will return immediately if there is no data to read
```

error_code : EWOULDBLOCK || EAGAIN

Syscall:
The following calls can be used to make the entire socket operation non-blocking:
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

# Raw Sockets:
- Requires root privileges. They bypass the transport layer and directly send packets.