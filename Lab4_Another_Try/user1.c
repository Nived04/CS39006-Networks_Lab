#include <stdio.h>
#include <ksocket.h>

char msg_data[504];
const char *eof_marker = "~";

int main(int argc, char *argv[])
{
    if(argc != 5){
        printf("Usage: %s <src_ip> <src_port> <dest_ip> <dest_port>\n", argv[0]);
        return -1;
    }

    const char *src_ip = argv[1];
    int src_port = atoi(argv[2]);
    const char *dest_ip = argv[3];
    int dest_port = atoi(argv[4]);

    int sockfd = k_socket(AF_INET, SOCK_KTP, 0);
    if (sockfd < 0) {
        perror("user1: k_socket");
        return -1;
    }

    if ((k_bind(sockfd, src_ip, src_port, dest_ip, dest_port)) < 0) {
        perror("user1: k_bind");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(dest_port);
    addr.sin_addr.s_addr = inet_addr(dest_ip);

    // FILE *fp = fopen("lorem_10KB.txt", "r");
    FILE *fp = fopen("Lorem.txt", "r");
    if (fp == NULL) {
        perror("user1: fopen");
        return -1;
    }

    sleep(2);
    while (1) {
        bzero(&msg_data, MAX_MESSAGE_SIZE - 8);
        // int bytesRead = fread(msg_data, 1, MAX_MESSAGE_SIZE-8, fp);

        int cnt = 0;
        while((cnt < MAX_MESSAGE_SIZE - 8)) {
            msg_data[cnt] = fgetc(fp);
            // printf("%d\n", msg_data[cnt]);
            cnt++;
            if(msg_data[cnt-1] == EOF) {
                cnt--;
                break;
            }
        }
        int bytesRead = cnt;
        printf("\nuser1: Read %d bytes.\n", bytesRead);
        
        printf("\nuser1: Read ------\n");
        fwrite(msg_data, 1, bytesRead, stdout);
        printf("\n------\n");

        // printf("\nuser1: Read ------\n%s\n------\n", msg_data);

        int numbytes;
        while(1) {
            numbytes = k_sendto(sockfd, msg_data, bytesRead, 0, (struct sockaddr *)&addr, sizeof(addr));
            
            if (numbytes < 0) {
                perror("user1: k_send");
                if (errno != ENOSPACE) {
                    fclose(fp);
                    k_close(sockfd);
                    return -1;
                }
                else {
                    sleep(1);
                }
            }
            else 
                break;
        }

        printf("user1: Sent %d bytes.\n", numbytes);

        if(feof(fp)) {
            printf("user1: End of file reached.\n");
            while(true) {
                char msg_end[1] = "~";
                numbytes = k_sendto(sockfd, msg_end, 1, 0, (struct sockaddr *)&addr, sizeof(addr));
                
                if (numbytes < 0) {
                    perror("user1: k_send");
                    if(errno != ENOSPACE) {
                        fclose(fp);
                        k_close(sockfd);
                        return -1;
                    }
                    else {
                        sleep(1);
                    }
                }
                else {
                    break;
                }
            }
            break;
        }
        
        // printf("user1: Read \n\n%s\n\n", msg.content.data.data);
        // if (bytesRead == 0) {
        //     if (feof(fp)) {
        //         printf("user1: End of cfile reached.\n");
        //         memcpy(msg_data, eof_marker, 1);
        //         bytesRead = 1;
        //     }
        //     else {
        //         perror("user1: fread");
        //         fclose(fp);
        //         return -1;
        //     }
        // }

        // int numbytes;
        // while(1) {
        //     numbytes = k_sendto(sockfd, msg_data, bytesRead+8, 0, (struct sockaddr *)&addr, sizeof(addr));
            
        //     if (numbytes < 0) {
        //         perror("user1: k_send");
        //         if (errno != ENOSPACE) {
        //             fclose(fp);
        //             k_close(sockfd);
        //             return -1;
        //         }
        //         else {
        //             sleep(1);
        //         }
        //     }

        //     break;
        // }

        // printf("user1: Sent %d bytes.\n", numbytes);

        // if(memcmp(msg_data, eof_marker, 1) == 0) {
        //     printf("user1: EOF marker sent.\n");
        //     break;
        // }
    }

    fclose(fp);

    sleep(20);

    k_close(sockfd);
    return 0;
}