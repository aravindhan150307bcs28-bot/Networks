
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT      5001
#define BUF_SIZE  1024

int main(int argc, char *argv[]) {
    int sockfd;
    char buffer[BUF_SIZE];
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    const char *server_ip = (argc > 1) ? argv[1] : "127.0.0.1";

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid server IP");
        exit(EXIT_FAILURE);
    }

    printf("[Chat-Client] Connected (UDP, iterative) to %s:%d\n", server_ip, PORT);
    printf("Type 'bye' to end the chat.\n\n");

    while (1) {
        printf("You     >> ");
        if (fgets(buffer, BUF_SIZE, stdin) == NULL) break;
        buffer[strcspn(buffer, "\n")] = '\0';

        sendto(sockfd, buffer, strlen(buffer), 0,
               (struct sockaddr *)&server_addr, addr_len);

        if (strcmp(buffer, "bye") == 0) {
            int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                              (struct sockaddr *)&server_addr, &addr_len);
            if (n > 0) { buffer[n] = '\0'; printf("Server  >> %s\n", buffer); }
            break;
        }

        int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                          (struct sockaddr *)&server_addr, &addr_len);
        if (n < 0) { perror("recvfrom() failed"); break; }
        buffer[n] = '\0';
        printf("Server  >> %s\n", buffer);
    }

    close(sockfd);
    return 0;
}
