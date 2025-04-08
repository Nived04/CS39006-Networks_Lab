## Custom Lightweight Discovery Protocol - CLDP

### Usage:
- run the command `make all` to get the executable files for the cldp client and servers.
- run the executables using the commands: `sudo ./cldp_server` and  `sudo ./cldp_client`

### Verfication:
Run wireshark in `any` mode and use the filter: `ip.proto == 253` to get the packets using the custom protocol

### Sample text output:
Client:
```
Received HELLO from xxx.xxx.xxx.xxx

Querying all active nodes...
Sent QUERY to xxx.xxx.xxx.xxx

Received RESPONSE from xxx.xxx.xxx.xxx:
Time: 2025-03-30 15:30:00 | Host: swlab-Networks-Assignment7 | Free RAM: 8000MB | CPU Load: 0.50%
```

Server:
```
HELLO announcement broadcasted

Received QUERY from xxx.xxx.xxx.xxx
Sent RESPONSE to xxx.xxx.xxx.xxx
```