#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[])
{
    int sockfd;
    struct sockaddr_in server_addr;

    char ip[BUF_SIZE];
    char buffer[BUF_SIZE];

    if (argc != 3)
    {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    /* Create TCP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return 1;
    }

    /* Server address setup */
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));

    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0)
    {
        printf("Invalid server IP address.\n");
        close(sockfd);
        return 1;
    }

    /* Connect to server */
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        return 1;
    }

    printf("Connected to ARP server.\n");

    /* Get IP address from terminal */
    printf("Enter IP address: ");
    fflush(stdout); // FIX 1: Ensures the prompt prints to the screen immediately

    fgets(ip, sizeof(ip), stdin);
    ip[strcspn(ip, "\r\n")] = '\0';

    /* Send IP address to server (including the null-terminator for TCP safety) */
    // FIX 2: Sending strlen(ip) + 1 passes the '\0' boundary down the wire
    send(sockfd, ip, strlen(ip) + 1, 0);

    /* Receive result from server */
    int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0)
    {
        buffer[n] = '\0';
        printf("\nServer Reply:\n%s", buffer);
    }
    else if (n == 0)
    {
        printf("\nServer closed the connection prematurely.\n");
    }
    else
    {
        perror("recv failed");
    }

    close(sockfd);
    return 0;
}
