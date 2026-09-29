#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#define BUFFER_SIZE 1024
int main(int argc, char *argv[])
{
int server_fd, client_fd; // Primary host entry gate and unique endpoint nodes
struct sockaddr_in server_addr, client_addr; // Layout interface address objects
char buffer[BUFFER_SIZE]; // Storage context arrays for string manipulation
int port_num; // Variable parameter for mapping socket interface configurations
socklen_t addr_len; // Width tracking metrics logic parameters
if (argc != 2)
{
fprintf(stderr, "Usage: %s <port>\n", argv[0]);
exit(EXIT_FAILURE);
}
port_num = atoi(argv[1]);
if (port_num <= 0 || port_num > 65535)
{
fprintf(stderr, "Invalid port number: %s\n", argv[1]);
exit(EXIT_FAILURE);
}
server_fd = socket(AF_INET, SOCK_STREAM, 0);
if (server_fd < 0)
{
perror("socket failed");
exit(EXIT_FAILURE);
}
memset(&server_addr, 0, sizeof(server_addr));
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(port_num);
server_addr.sin_addr.s_addr = INADDR_ANY;
if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
{
perror("bind failed");
close(server_fd);
exit(EXIT_FAILURE);
}
listen(server_fd, 5);
printf("TCP Chat Server listening on port %d...\n", port_num);
printf("Waiting for clients...\n\n");
int current_client_id = 0;
while (1)
{
addr_len = sizeof(client_addr);
client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
if (client_fd < 0)
{
perror("accept failed");
continue;
}
current_client_id++;
printf("Client %d connected.\n", current_client_id);
if (fork() == 0) // Spin sub-process loops
{
close(server_fd);
char chat_message[BUFFER_SIZE];
sprintf(chat_message, "You are Client %d", current_client_id);
send(client_fd, chat_message, strlen(chat_message), 0);
while (1)
{
int received_bytes = recv(client_fd, buffer, BUFFER_SIZE - 1, 0);
if (received_bytes <= 0)
break;
buffer[received_bytes] = '\0';
buffer[strcspn(buffer, "\n")] = '\0';
if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "/quit") == 0)
break;
printf("Client %d: %s\n", current_client_id, buffer);
printf("You(Server): ");
if (fgets(chat_message, BUFFER_SIZE, stdin) == NULL)
{
strcpy(chat_message, "quit");
}
chat_message[strcspn(chat_message, "\n")] = '\0';
char full_reply[BUFFER_SIZE];
sprintf(full_reply, "You(Server): %s", chat_message);
send(client_fd, full_reply, strlen(full_reply), 0);
if (strcmp(chat_message, "quit") == 0 || strcmp(chat_message, "/quit") == 0)
break;
}
printf("Client %d has left the chat.\n", current_client_id);
close(client_fd);
exit(0);
}
close(client_fd);
waitpid(-1, NULL, WNOHANG); // Wipe internal zombie tracking entities
}
close(server_fd);
return 0;
}
