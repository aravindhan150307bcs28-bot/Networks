#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT       9002
#define BUF_SIZE   256
#define POOL_SIZE  10

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int  leased;                 /* 0 = free, 1 = leased */
    char client_id[64];          /* "ip:port" of the leaseholder */
} Lease;

Lease pool[POOL_SIZE];

void init_pool(void) {
    /* Pool: 192.168.1.100 - 192.168.1.109 */
    for (int i = 0; i < POOL_SIZE; i++) {
        snprintf(pool[i].ip, sizeof(pool[i].ip), "192.168.1.%d", 100 + i);
        pool[i].leased = 0;
        pool[i].client_id[0] = '\0';
    }
}

void print_pool(void) {
    printf("\n----- DHCP Address Pool -----\n");
    for (int i = 0; i < POOL_SIZE; i++)
        printf("  %-15s : %s%s%s\n", pool[i].ip,
               pool[i].leased ? "LEASED to " : "FREE",
               pool[i].leased ? pool[i].client_id : "",
               "");
    printf("------------------------------\n\n");
}

int find_free_slot(void) {
    for (int i = 0; i < POOL_SIZE; i++)
        if (!pool[i].leased) return i;
    return -1;
}

int find_ip_slot(const char *ip) {
    for (int i = 0; i < POOL_SIZE; i++)
        if (strcmp(pool[i].ip, ip) == 0) return i;
    return -1;
}

int main(void) {
    int sockfd;
    char buffer[BUF_SIZE], reply[BUF_SIZE], client_id[64];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    init_pool();

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket() failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind() failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("[DHCP-Server] Listening on UDP port %d (iterative)\n", PORT);
    print_pool();

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                          (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) { perror("recvfrom() failed"); continue; }
        buffer[n] = '\0';

        snprintf(client_id, sizeof(client_id), "%s:%d",
                 inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        printf("[%s] -> %s\n", client_id, buffer);

        if (strcmp(buffer, "DISCOVER") == 0) {
            int idx = find_free_slot();
            if (idx == -1) {
                snprintf(reply, sizeof(reply), "NAK no_free_ip");
            } else {
                snprintf(reply, sizeof(reply), "OFFER %s", pool[idx].ip);
            }
            printf("            <- %s\n\n", reply);

        } else if (strncmp(buffer, "REQUEST ", 8) == 0) {
            char req_ip[INET_ADDRSTRLEN];
            sscanf(buffer + 8, "%15s", req_ip);
            int idx = find_ip_slot(req_ip);
            if (idx != -1 && !pool[idx].leased) {
                pool[idx].leased = 1;
                strncpy(pool[idx].client_id, client_id, sizeof(pool[idx].client_id) - 1);
                snprintf(reply, sizeof(reply), "ACK %s", req_ip);
                printf("            <- %s   (lease granted)\n", reply);
                print_pool();
            } else {
                snprintf(reply, sizeof(reply), "NAK %s", req_ip);
                printf("            <- %s   (unavailable)\n\n", reply);
            }

        } else if (strncmp(buffer, "RELEASE ", 8) == 0) {
            char rel_ip[INET_ADDRSTRLEN];
            sscanf(buffer + 8, "%15s", rel_ip);
            int idx = find_ip_slot(rel_ip);
            if (idx != -1) {
                pool[idx].leased = 0;
                pool[idx].client_id[0] = '\0';
            }
            snprintf(reply, sizeof(reply), "RELEASED %s", rel_ip);
            printf("            <- %s\n", reply);
            print_pool();

        } else {
            snprintf(reply, sizeof(reply), "ERROR unknown_command");
        }

        sendto(sockfd, reply, strlen(reply), 0,
               (struct sockaddr *)&client_addr, addr_len);
    }

    close(sockfd);
    return 0;
}
