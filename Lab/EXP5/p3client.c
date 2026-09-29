#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Convert each character into 8-bit binary */
void convertBinary(const char *str, char *result)
{
    int pos = 0;

    for (int i = 0; str[i] != '\0'; i++)
    {
        unsigned char value = str[i];

        for (int bit = 7; bit >= 0; bit--)
        {
            result[pos++] = ((value & (1 << bit)) != 0) ? '1' : '0';
        }
    }

    result[pos] = '\0';
}

/* Insert 0 after five continuous 1s */
void stuffBits(const char *data, char *output)
{
    int ones = 0;
    int index = 0;

    for (int i = 0; data[i] != '\0'; i++)
    {
        output[index++] = data[i];

        if (data[i] == '1')
        {
            ones++;

            if (ones == 5)
            {
                output[index++] = '0';
                ones = 0;
            }
        }
        else
        {
            ones = 0;
        }
    }

    output[index] = '\0';
}

int main()
{
    int client, option;
    char input[256];
    char binaryData[2048];
    char stuffedData[4096];
    char response[256];

    struct sockaddr_in server;

    /* Create socket */
    client = socket(AF_INET, SOCK_STREAM, 0);

    if (client == -1)
    {
        printf("Unable to create socket\n");
        return 1;
    }

    /* Server details */
    server.sin_family = AF_INET;
    server.sin_port = htons(5002);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    /* Connect to server */
    if (connect(client,
                (struct sockaddr *)&server,
                sizeof(server)) == -1)
    {
        printf("Unable to connect to server\n");
        close(client);
        return 1;
    }

    while (1)
    {
        printf("\n1. Send Message\n");
        printf("2. Exit\n");
        printf("Enter choice: ");
        scanf("%d", &option);
        getchar();

        if (option == 2)
        {
            break;
        }

        if (option != 1)
        {
            printf("Invalid choice\n");
            continue;
        }

        printf("Enter text message: ");
        fgets(input, sizeof(input), stdin);

        input[strcspn(input, "\n")] = '\0';

        /* Convert text to binary */
        convertBinary(input, binaryData);

        printf("Binary Representation: %s\n", binaryData);

        /* Perform bit stuffing */
        stuffBits(binaryData, stuffedData);

        printf("Data after Bit Stuffing: %s\n", stuffedData);

        /* Send stuffed data */
        write(client, stuffedData, strlen(stuffedData));

        /* Receive response */
        int received = read(client,
                            response,
                            sizeof(response) - 1);

        if (received > 0)
        {
            response[received] = '\0';

            printf("Server Response (Recovered Text): %s\n",
                   response);
        }
    }

    close(client);

    return 0;
}
