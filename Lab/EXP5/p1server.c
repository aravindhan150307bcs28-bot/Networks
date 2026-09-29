#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main()
{
    int sfd, cfd;
    struct sockaddr_in server;
    char text[100];

    sfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sfd < 0)
    {
        printf("Socket creation failed\n");
        return 1;
    }

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(6000);

    if (bind(sfd, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        printf("Bind failed\n");
        close(sfd);
        return 1;
    }

    if (listen(sfd, 1) < 0)
    {
        printf("Listen failed\n");
        close(sfd);
        return 1;
    }

    printf("TCP Echo Server is running...\n");

    cfd = accept(sfd, NULL, NULL);

    if (cfd < 0)
    {
        printf("Client connection failed\n");
        close(sfd);
        return 1;
    }

    int n = read(cfd, text, sizeof(text) - 1);

    if (n > 0)
    {
        text[n] = '\0';

        printf("Client: %s\n", text);

        write(cfd, text, strlen(text));

        printf("Echo sent to client\n");
    }

    close(cfd);
    close(sfd);

    return 0;
}
