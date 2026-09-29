#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT        9001
#define BUF_SIZE    512
#define TABLE_SIZE  53          /* prime bucket count */

/* ---------- Hash table (chaining) ---------- */
typedef struct Node {
    char domain[128];
    char ip[INET_ADDRSTRLEN];
    struct Node *next;
} Node;

Node *table[TABLE_SIZE];

/* djb2-style string hash */
unsigned int hash(const char *str) {
    unsigned long h = 5381;
    int c;
    while ((c = *str++))
        h = ((h << 5) + h) + c;      /* h * 33 + c */
    return (unsigned int)(h % TABLE_SIZE);
}

void insert(const char *domain, const char *ip) {
    unsigned int idx = hash(domain);
    Node *n = malloc(sizeof(Node));
    strncpy(n->domain, domain, sizeof(n->domain) - 1);
    n->domain[sizeof(n->domain) - 1] = '\0';
    strncpy(n->ip, ip, sizeof(n->ip) - 1);
    n->ip[sizeof(n->ip) - 1] = '\0';
    n->next = table[idx];         /* insert at head of bucket */
    table[idx] = n;
}

/* Returns pointer to IP string, or NULL if not found */
const char *lookup(const char *domain) {
    unsigned int idx = hash(domain);
    Node *cur = table[idx];
    while (cur) {
        if (strcmp(cur->domain, domain) == 0)
            return cur->ip;
        cur = cur->next;
    }
    return NULL;
}

void load_records(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("[DNS-Server] records.txt not found, loading built-in defaults.\n");
        insert("google.com",     "142.250.183.14");
        insert("youtube.com",    "142.250.183.78");
        insert("facebook.com",   "157.240.22.35");
        insert("github.com",     "140.82.112.3");
        insert("wikipedia.org",  "208.80.154.224");
        insert("amazon.com",     "205.251.242.103");
        insert("openai.com",     "104.18.32.115");
        return;
    }
    char domain[128], ip[64];
    while (fscanf(fp, "%127s %63s", domain, ip) == 2) {
        insert(domain, ip);
    }
    fclose(fp);
}

void print_table(void) {
    printf("\n----- DNS Hash Table (%d buckets) -----\n", TABLE_SIZE);
    for (int i = 0; i < TABLE_SIZE; i++) {
        if (!table[i]) continue;
        printf("bucket[%2d]: ", i);
        for (Node *cur = table[i]; cur; cur = cur->next)
            printf("(%s -> %s) ", cur->domain, cur->ip);
        printf("\n");
    }
    printf("----------------------------------------\n\n");
}

int main(void) {
    int sockfd;
    char buffer[BUF_SIZE];
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);

    load_records("records.txt");
    print_table();

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

    printf("[DNS-Server] Listening on UDP port %d (iterative)\n\n", PORT);

    while (1) {
        memset(buffer, 0, BUF_SIZE);
        int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                          (struct sockaddr *)&client_addr, &addr_len);
        if (n < 0) { perror("recvfrom() failed"); continue; }
        buffer[n] = '\0';

        printf("Query from %s:%d  ->  domain = \"%s\"\n",
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), buffer);

        const char *ip = lookup(buffer);
        char reply[BUF_SIZE];
        if (ip) {
            snprintf(reply, sizeof(reply), "%s", ip);
            printf("  -> RESOLVED  : %s = %s\n\n", buffer, ip);
        } else {
            snprintf(reply, sizeof(reply), "NXDOMAIN");
            printf("  -> NOT FOUND : %s is not in the table\n\n", buffer);
        }

        sendto(sockfd, reply, strlen(reply), 0,
               (struct sockaddr *)&client_addr, addr_len);
    }

    close(sockfd);
    return 0;
}
