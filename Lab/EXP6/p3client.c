#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT      9002
#define BUF_SIZE  256

static int udp_exchange(int sockfd, struct sockaddr_in *server_addr,
                         socklen_t addr_len, const char *msg, char *reply) {
    sendto(sockfd, msg, strlen(msg), 0, (struct sockaddr *)server_addr, addr_len);
    int n = recvfrom(sockfd, reply, BUF_SIZE - 1, 0,
                      (struct sockaddr *)server_addr, &addr_len);
    if (n < 0) return -1;
    reply[n] = '\0';
    return 0;
}

int main(int argc, char *argv[]) {
    int sockfd;
    char reply[BUF_SIZE], request[BUF_SIZE], leased_ip[64] = {0};
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

    printf("[DHCP-Client] Starting DORA exchange with %s:%d\n\n", server_ip, PORT);

    /* --- DISCOVER --- */
    printf("Step 1: Sending DISCOVER...\n");
    if (udp_exchange(sockfd, &server_addr, addr_len, "DISCOVER", reply) < 0) {
        perror("recvfrom failed"); exit(EXIT_FAILURE);
    }
    printf("Step 2: Received %s\n", reply);

    if (strncmp(reply, "OFFER ", 6) != 0) {
        printf("No IP offered by server. Exiting.\n");
        close(sockfd);
        return 1;
    }
    sscanf(reply + 6, "%63s", leased_ip);

    /* --- REQUEST --- */
    snprintf(request, sizeof(request), "REQUEST %s", leased_ip);
    printf("Step 3: Sending %s\n", request);
    if (udp_exchange(sockfd, &server_addr, addr_len, request, reply) < 0) {
        perror("recvfrom failed"); exit(EXIT_FAILURE);
    }
    printf("Step 4: Received %s\n\n", reply);

    if (strncmp(reply, "ACK ", 4) == 0)
        printf(">>> Lease successful! Assigned IP address: %s\n\n", leased_ip);
    else
        printf(">>> Lease failed for %s\n\n", leased_ip);

    printf("Type 'release' to give the IP back, or anything else to quit: ");
    char cmd[32];
    if (fgets(cmd, sizeof(cmd), stdin) && strncmp(cmd, "release", 7) == 0) {
        snprintf(request, sizeof(request), "RELEASE %s", leased_ip);
        udp_exchange(sockfd, &server_addr, addr_len, request, reply);
        printf("%s\n", reply);
    }

    close(sockfd);
    return 0;
}
