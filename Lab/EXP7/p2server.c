#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h> // Thread interface standard tracking components
#define BUF_SIZE 1024
#define TABLE_SIZE 10
/* Simulated ARP entry profile layout */
struct ARP_Entry
{
char ip_addr[20]; // IP address value key parameters
char mac_addr[20]; // Mapped dynamic physical signature string context
int is_occupied; // Slot engagement tracker boolean flag status
};
struct ARP_Entry arp_table[TABLE_SIZE]; // Global simulation hash framework layout matrix
pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER; // Exclusion element lock tracker components
/* Hash value transformation index tracker */
int compute_hash(char ip[])
{
int character_sum = 0;
for (int i = 0; ip[i] != '\0'; i++)
character_sum = character_sum + ip[i];
return character_sum % TABLE_SIZE;
}
/* Linear probe sequence insertions mapping entries logic tracking handlers */
void insert_arp(char ip[], char mac[])
{
int table_index = compute_hash(ip);
int starting_bound = table_index;
while (arp_table[table_index].is_occupied == 1)
{
table_index = (table_index + 1) % TABLE_SIZE;
if (table_index == starting_bound) {
printf("ARP Table is full! Cannot insert %s\n", ip);
return;
}
}
strcpy(arp_table[table_index].ip_addr, ip);
strcpy(arp_table[table_index].mac_addr, mac);
arp_table[table_index].is_occupied = 1;
}
/* Mapping key lookup parameters entries parser trackers handlers profiles */
int search_arp(char ip[], char mac_collector[])
{
int table_index = compute_hash(ip);
int starting_bound = table_index;
while (arp_table[table_index].is_occupied == 1)
{
if (strcmp(arp_table[table_index].ip_addr, ip) == 0)
{
strcpy(mac_collector, arp_table[table_index].mac_addr);
return 1;
}
table_index = (table_index + 1) % TABLE_SIZE;
if (table_index == starting_bound)
break;
}
return 0;
}
/* Fake format hardware assignment strings compilers metrics tracking generator code logic profiles */
void generate_mac_addr(char ip[], char mac_buffer[])
{
int part_1, part_2, part_3, part_4;
if (sscanf(ip, "%d.%d.%d.%d", &part_1, &part_2, &part_3, &part_4) == 4) {
sprintf(mac_buffer, "AA:BB:%02X:%02X:%02X:%02X", part_1, part_2, part_3, part_4);
} else {
strcpy(mac_buffer, "00:00:00:00:00:00");
}
}
/* Interface display dump compiler tracker structures mappings details profile formats */
void print_arp_table()
{
printf("\n============ CURRENT SIMULATED ARP TABLE ============\n");
printf("%-5s %-18s %-20s\n", "Slot", "IP Address", "MAC Address");
printf("-----------------------------------------------------\n");
for (int i = 0; i < TABLE_SIZE; i++)
{
if (arp_table[i].is_occupied == 1) {
printf("[%02d] %-18s %-20s\n", i, arp_table[i].ip_addr, arp_table[i].mac_addr);
} else {
printf("[%02d] %-18s %-20s\n", i, "[Empty]", "None");
}
}
printf("=====================================================\n\n");
}
/* Concurrent worker interface parsing tracker loops configurations modules context */
void *client_handler(void *arg)
{
int client_fd = *(int *)arg;
free(arg); // Clear dynamic memory tracking segments immediately
char input_buffer[BUF_SIZE];
char calculated_mac[20];
char reply_message[BUF_SIZE];
int read_bytes = recv(client_fd, input_buffer, sizeof(input_buffer) - 1, 0);
if (read_bytes > 0)
{
input_buffer[read_bytes] = '\0';
input_buffer[strcspn(input_buffer, "\r\n")] = '\0';
struct in_addr test_address;
if (inet_pton(AF_INET, input_buffer, &test_address) != 1)
{
char *error_response = "Invalid IP address\n";
send(client_fd, error_response, strlen(error_response), 0);
}
else
{
pthread_mutex_lock(&table_mutex); // Lock resource state metrics changes
if (search_arp(input_buffer, calculated_mac))
{
printf("IP found in ARP table.\n");
sprintf(reply_message, "IP: %s\nMAC: %s\nIP found in ARP table.\n", input_buffer, calculated_mac);
}
else
{
generate_mac_addr(input_buffer, calculated_mac);
insert_arp(input_buffer, calculated_mac);
printf("IP not found. Added to ARP table.\n");
sprintf(reply_message, "IP: %s\nMAC: %s\nIP not found. Added to ARP table.\n", input_buffer, calculated_mac);
}
print_arp_table(); // Display table adjustments from inside thread lock scope safety guidelines context
pthread_mutex_unlock(&table_mutex); // Clear transaction boundaries components exclusions tags
send(client_fd, reply_message, strlen(reply_message), 0);
}
}
close(client_fd);
return NULL;
}
int main(int argc, char *argv[])
{
int server_fd, client_fd; // Master gate controller and current runtime worker link mappings points para
struct sockaddr_in server_addr, client_addr; // Router structural configurations layouts parameters objects
socklen_t addr_len; // Holds sizes criteria contexts variables params parameters width trackers ma
if (argc != 2)
{
printf("Usage: %s <port>\n", argv[0]);
return 1;
}
server_fd = socket(AF_INET, SOCK_STREAM, 0);
if (server_fd < 0)
{
perror("socket");
return 1;
}
int socket_option = 1;
setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &socket_option, sizeof(socket_option)); // Clears "Address already in use"
server_addr.sin_family = AF_INET;
server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
server_addr.sin_port = htons(atoi(argv[1]));
if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
{
perror("bind");
close(server_fd);
return 1;
}
if (listen(server_fd, 5) < 0)
{
perror("listen");
close(server_fd);
return 1;
}
/* Seed lookup tracking elements */
insert_arp("192.168.1.1", "AA:BB:CC:DD:EE:01");
insert_arp("192.168.1.2", "AA:BB:CC:DD:EE:02");
insert_arp("192.168.1.3", "AA:BB:CC:DD:EE:03");
insert_arp("192.168.1.4", "AA:BB:CC:DD:EE:04");
printf("ARP lookup server running on port %s...\n", argv[1]);
print_arp_table();
while (1)
{
addr_len = sizeof(client_addr);
client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
if (client_fd < 0)
{
perror("accept");
continue;
}
printf("Client connected.\n");
int *socket_pointer = malloc(sizeof(int)); // Allocate pointers data states segments metrics locations tracking areas
if (socket_pointer == NULL) {
perror("malloc failed");
close(client_fd);
continue;
}
*socket_pointer = client_fd;
pthread_t thread_id; // Thread entity identifier structure label parameters variables mappings properties tags
if (pthread_create(&thread_id, NULL, client_handler, (void *)socket_pointer) != 0)
{
perror("pthread_create failed");
free(socket_pointer);
close(client_fd);
continue;
}
pthread_detach(thread_id); // Release operational resources immediately to background automatic maintenance systems man
}
close(server_fd);
return 0;
}
