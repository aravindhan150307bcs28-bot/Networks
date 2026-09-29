#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // Required for multi-threading

#define BUF_SIZE 1024
#define TABLE_SIZE 10

/* ARP table entry */
struct ARP
{
    char ip[20];
    char mac[20];
    int used;
};

/* ARP hash table and Mutex Lock */
struct ARP table[TABLE_SIZE];
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Simple hash function */
int hash(char ip[])
{
    int value = 0;
    for (int i = 0; ip[i] != '\0'; i++)
        value = value + ip[i];
    return value % TABLE_SIZE;
}

/* Insert IP and MAC into hash table */
void insert(char ip[], char mac[])
{
    int index = hash(ip);
    int start = index;

    while (table[index].used == 1)
    {
        index = (index + 1) % TABLE_SIZE;
        if (index == start) {
            printf("ARP Table is full! Cannot insert %s\n", ip);
            return;
        }
    }

    strcpy(table[index].ip, ip);
    strcpy(table[index].mac, mac);
    table[index].used = 1;
}

/* Search IP in hash table */
int search(char ip[], char mac[])
{
    int index = hash(ip);
    int start = index;

    while (table[index].used == 1)
    {
        if (strcmp(table[index].ip, ip) == 0)
        {
            strcpy(mac, table[index].mac);
            return 1;
        }

        index = (index + 1) % TABLE_SIZE;

        if (index == start)
            break;
    }
    return 0;
}

/* New Function: Search target IP directly inside the real system ARP/neighbor cache */
int search_system_arp_cache(char target_ip[], char found_mac[])
{
    FILE *fp = popen("ip neigh", "r");
    if (fp == NULL)
    {
        perror("popen (ip neigh) failed");
        return 0;
    }

    char line[BUF_SIZE];
    int found = 0;

    // "ip neigh" outputs format: <IP> dev <interface> lladdr <MAC> <STATE>
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        char parsed_ip[40] = {0};
        char parsed_mac[40] = {0};
        char token[40] = {0};

        // Scan the line token by token to locate the IP and the physical link-layer address (lladdr)
        if (sscanf(line, "%39s", parsed_ip) == 1)
        {
            // If the IP on this line matches what the client asked for
            if (strcmp(parsed_ip, target_ip) == 0)
            {
                char *lladdr_ptr = strstr(line, "lladdr");
                if (lladdr_ptr != NULL)
                {
                    if (sscanf(lladdr_ptr, "lladdr %39s", parsed_mac) == 1)
                    {
                        strncpy(found_mac, parsed_mac, 19);
                        found_mac[19] = '\0';
                        found = 1;
                        break;
                    }
                }
            }
        }
    }

    pclose(fp);
    return found;
}

/* Generate a simple MAC address for a new IP */
void generate_mac(char ip[], char mac[])
{
    int a, b, c, d;
    if (sscanf(ip, "%d.%d.%d.%d", &a, &b, &c, &d) == 4) {
        sprintf(mac, "AA:BB:%02X:%02X:%02X:%02X", a, b, c, d);
    } else {
        strcpy(mac, "00:00:00:00:00:00");
    }
}

/* Helper function to view the user-space ARP table */
void print_arp_table()
{
    printf("\n============ CURRENT SIMULATED ARP TABLE ============\n");
    printf("%-5s %-18s %-20s\n", "Slot", "IP Address", "MAC Address");
    printf("-----------------------------------------------------\n");
    for (int i = 0; i < TABLE_SIZE; i++)
    {
        if (table[i].used == 1) {
            printf("[%02d]  %-18s %-20s\n", i, table[i].ip, table[i].mac);
        } else {
            printf("[%02d]  %-18s %-20s\n", i, "[Empty]", "None");
        }
    }
    printf("=====================================================\n\n");
}

/* Runs the "ip neigh" shell command to display the actual kernel ARP/neighbor cache */
void print_system_arp_cache()
{
    printf("\n============ SYSTEM ARP CACHE (ip neigh) ============\n");

    FILE *fp = popen("ip neigh", "r");
    if (fp == NULL)
    {
        perror("popen (ip neigh) failed");
        return;
    }

    char line[BUF_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), fp) != NULL)
    {
        printf("%s", line);
        found = 1;
    }

    if (!found)
    {
        printf("(No entries returned by 'ip neigh')\n");
    }

    pclose(fp);
    printf("=====================================================\n\n");
}

/* Thread handler function for concurrent clients */
void *client_handler(void *arg)
{
    int client_fd = *(int *)arg;
    free(arg); // Free the memory allocated in main for the client socket descriptor

    char buffer[BUF_SIZE];
    char mac[20];
    char reply[BUF_SIZE];

    /* Receive IP address */
    int n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

    if (n > 0)
    {
        buffer[n] = '\0';
        buffer[strcspn(buffer, "\r\n")] = '\0'; // Strip newline variants

        /* Check whether IP is valid */
        struct in_addr test_addr;
        if (inet_pton(AF_INET, buffer, &test_addr) != 1)
        {
            char *msg = "Invalid IP address\n";
            send(client_fd, msg, strlen(msg), 0);
        }
        else
        {
            // Lock the mutex before touching the shared table array or running cache checks
            pthread_mutex_lock(&table_mutex);

            if (search(buffer, mac))
            {
                printf("IP found in simulated ARP table.\n");
                sprintf(reply, "IP: %s\nMAC: %s\nIP found in simulated ARP table.\n", buffer, mac);
            }
            else if (search_system_arp_cache(buffer, mac))
            {
                // Discovered in the kernel's real neighbor cache! Let's save it to our table.
                insert(buffer, mac);
                printf("IP found in System Kernel Cache (ip neigh). Added to simulated table.\n");
                sprintf(reply, "IP: %s\nMAC: %s\nIP found in System Kernel Cache (ip neigh) & cached locally.\n", buffer, mac);
            }
            else
            {
                // Not found anywhere; fallback to auto-generating a fake address
                generate_mac(buffer, mac);
                insert(buffer, mac);
                printf("IP not found anywhere. Generated new MAC and added to table.\n");
                sprintf(reply, "IP: %s\nMAC: %s\nIP not found anywhere. Generated new fake MAC.\n", buffer, mac);
            }

            // Print table updates inside the lock for execution safety
            print_arp_table();

            // Also show the real kernel ARP/neighbor cache for comparison
            print_system_arp_cache();

            // Release the lock for other threads to use
            pthread_mutex_unlock(&table_mutex);

            /* Send result back to client */
            send(client_fd, reply, strlen(reply), 0);
        }
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t len;

    if (argc != 2)
    {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    /* Create TCP socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    // Allow immediate port reuse to bypass "bind: Address already in use" errors
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Server address config */
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(atoi(argv[1]));

    /* Bind */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* Listen */
    if (listen(server_fd, 5) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    memset(table, 0, sizeof(table));

    /* Initial ARP table seed entries */
    insert("192.168.1.1", "AA:BB:CC:DD:EE:01");
    insert("192.168.1.2", "AA:BB:CC:DD:EE:02");
    insert("192.168.1.3", "AA:BB:CC:DD:EE:03");
    insert("192.168.1.4", "AA:BB:CC:DD:EE:04");

    printf("ARP lookup server running on port %s...\n", argv[1]);
    print_arp_table();
    print_system_arp_cache();

    while (1)
    {
        len = sizeof(client_addr);

        /* Accept client connection */
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &len);
        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("Client connected.\n");

        // Safely pass file descriptor by dynamically allocating individual integers
        int *client_sock_ptr = malloc(sizeof(int));
        if (client_sock_ptr == NULL) {
            perror("malloc failed");
            close(client_fd);
            continue;
        }
        *client_sock_ptr = client_fd;

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, client_handler, (void *)client_sock_ptr) != 0)
        {
            perror("pthread_create failed");
            free(client_sock_ptr);
            close(client_fd);
            continue;
        }

        // Detach the thread so that system handles the resources automatically upon closure
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
