#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#define BUFFER_SIZE 1024
int main(int argc, char *argv[])
{
int socket_fd; // Primary network channel file descriptor
struct sockaddr_in server_addr; // Targeted node routing context
char buffer[BUFFER_SIZE]; // Text input and messaging field array
int port_num; // Explicit network service interface port
if (argc != 3)
{
fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
exit(EXIT_FAILURE);
}
port_num = atoi(argv[2]);
if (port_num <= 0 || port_num > 65535)
{
fprintf(stderr, "Invalid port number: %s\n", argv[2]);
exit(EXIT_FAILURE);
}
socket_fd = socket(AF_INET, SOCK_STREAM, 0);
if (socket_fd < 0)
{
perror("socket failed");
exit(EXIT_FAILURE);
}
memset(&server_addr, 0, sizeof(server_addr));
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(port_num);
if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
{
fprintf(stderr, "Invalid server IP address: %s\n", argv[1]);
close(socket_fd);
exit(EXIT_FAILURE);
}
if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
{
perror("connect failed");
close(socket_fd);
exit(EXIT_FAILURE);
}
printf("Connected to chat server at %s:%d\n", argv[1], port_num);
printf("Type 'quit' to end the chat.\n\n");
int received_count = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0); // Grab initial client ID metadata
if (received_count > 0)
{
buffer[received_count] = '\0';
printf("%s\n\n", buffer);
}
while (1)
{
printf("You: ");
if (fgets(buffer, BUFFER_SIZE, stdin) == NULL)
{
strcpy(buffer, "quit");
}
buffer[strcspn(buffer, "\n")] = '\0'; // Wipe layout control characters
if (send(socket_fd, buffer, strlen(buffer), 0) < 0)
{
perror("send failed");
break;
}
if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "/quit") == 0)
{
printf("You ended the chat.\n");
break;
}
received_count = recv(socket_fd, buffer, BUFFER_SIZE - 1, 0); // Catch reply statement strings
if (received_count <= 0)
{
printf("Server disconnected.\n");
break;
}
buffer[received_count] = '\0';
printf("%s\n\n", buffer);
if (strcmp(buffer, "quit") == 0 || strcmp(buffer, "/quit") == 0)
{
printf("Server ended the chat.\n");
break;
}
}
close(socket_fd);
return 0;
}
