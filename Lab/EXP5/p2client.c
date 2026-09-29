#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main()
{
    int clientSocket, option;
    int size, numbers[100], total;
    struct sockaddr_in serverAddr;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == -1)
    {
        printf("Unable to create socket\n");
        return 1;
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(5001);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(clientSocket,
                (struct sockaddr *)&serverAddr,
                sizeof(serverAddr)) == -1)
    {
        printf("Unable to connect to server\n");
        close(clientSocket);
        return 1;
    }

    do
    {
        printf("\n1. Find Sum of Array\n");
        printf("2. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &option);

        if (option == 1)
        {
            printf("Enter array size: ");
            scanf("%d", &size);

            printf("Enter array values:\n");

            for (int j = 0; j < size; j++)
            {
                scanf("%d", &numbers[j]);
            }

            write(clientSocket, &size, sizeof(int));
            write(clientSocket, numbers, sizeof(int) * size);

            read(clientSocket, &total, sizeof(int));

            printf("Sum received from server = %d\n", total);
        }
        else if (option != 2)
        {
            printf("Wrong choice\n");
        }

    } while (option != 2);

    close(clientSocket);

    return 0;
}
