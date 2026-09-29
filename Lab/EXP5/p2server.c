#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main()
{
    int listenSocket, connectedSocket;
    int count, values[100], answer;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientLen = sizeof(clientAddr);

    listenSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (listenSocket == -1)
    {
        printf("Unable to create server socket\n");
        return 1;
    }

    int enable = 1;

    setsockopt(listenSocket,
               SOL_SOCKET,
               SO_REUSEADDR,
               &enable,
               sizeof(enable));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(5001);

    if (bind(listenSocket,
             (struct sockaddr *)&serverAddr,
             sizeof(serverAddr)) == -1)
    {
        printf("Binding error\n");
        close(listenSocket);
        return 1;
    }

    if (listen(listenSocket, 5) == -1)
    {
        printf("Listening error\n");
        close(listenSocket);
        return 1;
    }

    printf("Array Sum Server Started...\n");

    while (1)
    {
        connectedSocket = accept(listenSocket,
                                 (struct sockaddr *)&clientAddr,
                                 &clientLen);

        if (connectedSocket == -1)
        {
            printf("Client connection failed\n");
            continue;
        }

        read(connectedSocket, &count, sizeof(int));

        read(connectedSocket,
             values,
             sizeof(int) * count);

        printf("Array received from client\n");

        answer = 0;

        for (int j = 0; j < count; j++)
        {
            answer = answer + values[j];
        }

        write(connectedSocket, &answer, sizeof(int));

        printf("Calculated Sum = %d\n", answer);

        close(connectedSocket);
    }

    close(listenSocket);

    return 0;
}
