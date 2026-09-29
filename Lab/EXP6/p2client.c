#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT      9001
#define BUF_SIZE  512

int main(int argc, char *argv[]) {
    int sockfd;
    char domain[BUF_SIZE], reply[BUF_SIZE];
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
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    printf("[DNS-Client] Resolver ready. Talking to DNS server at %s:%d\n", server_ip, PORT);
    printf("Type 'exit' to quit.\n\n");

    while (1) {
        printf("Enter domain name to resolve: ");
        if (fgets(domain, BUF_SIZE, stdin) == NULL) break;
        domain[strcspn(domain, "\n")] = '\0';

        if (strlen(domain) == 0) continue;
        if (strcmp(domain, "exit") == 0) break;

        sendto(sockfd, domain, strlen(domain), 0,
               (struct sockaddr *)&server_addr, addr_len);

        int n = recvfrom(sockfd, reply, BUF_SIZE - 1, 0,
                          (struct sockaddr *)&server_addr, &addr_len);
        if (n < 0) { perror("recvfrom() failed"); continue; }
        reply[n] = '\0';

        if (strcmp(reply, "NXDOMAIN") == 0)
            printf("  -> Could not resolve \"%s\" (not in DNS table)\n\n", domain);
        else
            printf("  -> %s resolves to IP address: %s\n\n", domain, reply);
    }

    close(sockfd);
    return 0;
}
