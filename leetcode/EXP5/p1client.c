#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFSZ 1024
#define SERVER_IP "127.0.0.1"

int main(void)
{
    int sockfd, n;
    struct sockaddr_in servaddr;
    char buff[BUFSZ];

    /* 1. create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    /* 2. server address */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr) <= 0) {
        perror("inet_pton"); exit(1);
    }

    /* 3. connect */
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("connect"); exit(1);
    }
    printf("Connected to palindrome server.\n");
    printf("Type a string (type 'exit' to quit)\n");

    for (;;) {
        printf("\nEnter string : ");
        if (!fgets(buff, BUFSZ, stdin)) break;
        buff[strcspn(buff, "\r\n")] = '\0';

        if (strlen(buff) == 0) continue;

        write(sockfd, buff, strlen(buff) + 1);      /* 4. send */

        if (strcmp(buff, "exit") == 0) break;

        n = read(sockfd, buff, BUFSZ - 1);          /* 5. receive */
        if (n <= 0) { printf("Server closed connection.\n"); break; }
        buff[n] = '\0';
        printf("Server says  : %s\n", buff);
    }

    close(sockfd);
    printf("Client terminated.\n");
    return 0;
}
