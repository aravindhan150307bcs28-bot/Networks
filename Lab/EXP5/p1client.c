#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

int main()
{
    int fd;
    struct sockaddr_in addr;
    char text[100];
    char result[100];

    fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0)
    {
        printf("Socket creation failed\n");
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(6000);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        printf("Server connection failed\n");
        close(fd);
        return 1;
    }

    printf("Enter message: ");
    scanf(" %[^\n]", text);

    write(fd, text, strlen(text));

    int n = read(fd, result, sizeof(result) - 1);

    if (n > 0)
    {
        result[n] = '\0';
        printf("Server Echo: %s\n", result);
    }

    close(fd);

    return 0;
}
