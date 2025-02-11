### Select():
Multiplexes between multiple file descriptors
select(nfds - no of file descriptors, rdfd, writefd, exptfd, timeout)
- Timeout 0 means infinite waiting (or until a fd becomes active)
- Timeout works like a counter. eg. if a fd becomes active at 5 sec, but timeout was 12 sec, the value of timeout will become 7 sec
 
### fd_set structure:
Creates a bit map.
 _ _ _ _ _ 
|_|_|_|_|_| - initially all 0

### FD_SET(fd, fd_set)

### FD_ISSET(fd)

fd_set readfds;
FD_ZERO(readfds);
FD_SET(sockfd, readfds);
FD_SET(stdin, readfds);

select(sockfd+1, readfds, NULL, NULL, timeout);

if(FD_ISSET(sockfd)) {
    recv();
}
else if(FD_ISSET(stdin)) {
    scanf();
}