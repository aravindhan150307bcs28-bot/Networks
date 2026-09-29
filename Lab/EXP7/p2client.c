#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#define BUF_SIZE 1024
int main(int argc, char *argv[])
{
int socket_fd; // Transmission interface descriptor identifier
struct sockaddr_in server_addr; // Router destination endpoint details
char input_ip[BUF_SIZE]; // Input targeting IP parameter address
char response_buffer[BUF_SIZE]; // Payload tracker storage arrays
if (argc != 3)
{
printf("Usage: %s <server_ip> <port>\n", argv[0]);
return 1;
}
socket_fd = socket(AF_INET, SOCK_STREAM, 0);
if (socket_fd < 0)
{
perror("socket");
return 1;
}
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(atoi(argv[2]));
if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
{
printf("Invalid server IP address.\n");
close(socket_fd);
return 1;
}
if (connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
{
perror("connect");
close(socket_fd);
return 1;
}
printf("Connected to ARP server.\n");
printf("Enter IP address: ");
fflush(stdout); // FIX 1: Ensures instant synchronization down standard console tracking channels
fgets(input_ip, sizeof(input_ip), stdin);
input_ip[strcspn(input_ip, "\r\n")] = '\0';
// FIX 2: Includes the trailing boundary token inside standard streaming transactions
send(socket_fd, input_ip, strlen(input_ip) + 1, 0);
int received_bytes = recv(socket_fd, response_buffer, sizeof(response_buffer) - 1, 0);
if (received_bytes > 0)
{
response_buffer[received_bytes] = '\0';
printf("\nServer Reply:\n%s", response_buffer);
}
else if (received_bytes == 0)
{
printf("\nServer closed the connection prematurely.\n");
}
else
{
perror("recv failed");
}
close(socket_fd);
return 0;
}
