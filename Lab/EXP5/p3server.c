#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Remove the extra 0 inserted during bit stuffing */
void removeStuffedBits(const char *input, char *output)
{
    int ones = 0;
    int p = 0;

    for (int i = 0; input[i] != '\0'; i++)
    {
        if (input[i] == '1')
        {
            output[p++] = '1';
            ones++;
        }
        else
        {
            if (ones == 5)
            {
                ones = 0;
                continue;
            }

            output[p++] = '0';
            ones = 0;
        }
    }

    output[p] = '\0';
}

/* Convert groups of 8 binary bits into characters */
void convertToText(const char *bits, char *message)
{
    int length = strlen(bits);
    int position = 0;

    for (int start = 0; start < length; start += 8)
    {
        unsigned char value = 0;

        for (int bit = 0; bit < 8; bit++)
        {
            value <<= 1;

            if (start + bit < length &&
                bits[start + bit] == '1')
            {
                value |= 1;
            }
        }

        message[position++] = value;
    }

    message[position] = '\0';
}

int main()
{
    int listener, connection;
    char received[2048];
    char cleanData[2048];
    char message[1024];

    struct sockaddr_in serverAddress;
    socklen_t length = sizeof(serverAddress);

    /* Create TCP socket */
    listener = socket(AF_INET, SOCK_STREAM, 0);

    if (listener == -1)
    {
        printf("Unable to create socket\n");
        return 1;
    }

    /* Reuse the port */
    int reuse = 1;

    setsockopt(listener,
               SOL_SOCKET,
               SO_REUSEADDR,
               &reuse,
               sizeof(reuse));

    /* Configure server */
    memset(&serverAddress, 0, sizeof(serverAddress));

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(5002);

    /* Bind */
    if (bind(listener,
             (struct sockaddr *)&serverAddress,
             sizeof(serverAddress)) == -1)
    {
        printf("Unable to bind socket\n");
        close(listener);
        return 1;
    }

    /* Listen */
    if (listen(listener, 5) == -1)
    {
        printf("Unable to listen\n");
        close(listener);
        return 1;
    }

    printf("TCP Bit Stuffing Server is running...\n");

    while (1)
    {
        /* Accept client */
        connection = accept(listener,
                            (struct sockaddr *)&serverAddress,
                            &length);

        if (connection == -1)
        {
            printf("Client connection failed\n");
            continue;
        }

        /* Receive stuffed data */
        int count = read(connection,
                         received,
                         sizeof(received) - 1);

        if (count > 0)
        {
            received[count] = '\0';

            printf("\nReceived Stuffed Binary Data: %s\n",
                   received);

            /* Remove stuffed bits */
            removeStuffedBits(received, cleanData);

            printf("Destuffed Binary Data: %s\n",
                   cleanData);

            /* Convert binary into text */
            convertToText(cleanData, message);

            printf("Recovered Text Message: %s\n",
                   message);

            /* Send recovered message */
            write(connection,
                  message,
                  strlen(message));

            printf("Echoed recovered text back to client.\n");
        }

        close(connection);
    }

    close(listener);

    return 0;
}
