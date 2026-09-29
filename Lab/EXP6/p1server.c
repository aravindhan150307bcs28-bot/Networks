#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT      5001
#define BUF_SIZE  1024

int main(void) {
    int sockfd;
    char buffer[BUF_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    /* 1. Create UDP socket */
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    /* 2. Bind to well known port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[Chat-Server] Listening on UDP port %d (iterative)\n", PORT);
    printf("[Chat-Server] Waiting for a client to say hello...\n\n");

    /* 3. Iterative request/response loop - one exchange at a time */
    while (1) {
        memset(buffer, 0, BUF_SIZE);
        int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                          (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom() failed");
            continue;
        }
        buffer[n] = '\0';

        printf("Client [%s:%d] >> %s\n",
               inet_ntoa(client_addr.sin_addr),
               ntohs(client_addr.sin_port), buffer);

        if (strcmp(buffer, "bye") == 0) {
            const char *msg = "Server: Goodbye!";
            sendto(sockfd, msg, strlen(msg), 0,
                   (struct sockaddr *)&client_addr, addr_len);
            printf("Client disconnected.\n\n");
            continue; /* server keeps running for the next client */
        }

        printf("Server  >> ");
        fflush(stdout);
        if (fgets(buffer, BUF_SIZE, stdin) == NULL) break;
        buffer[strcspn(buffer, "\n")] = '\0';

        sendto(sockfd, buffer, strlen(buffer), 0,
               (struct sockaddr *)&client_addr, addr_len);
    }

    close(sockfd);
    return 0;
}
