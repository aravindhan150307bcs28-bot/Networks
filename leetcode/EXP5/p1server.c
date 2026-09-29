#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define PORT 8080
#define BUFSZ 1024

/* returns 1 if palindrome, 0 otherwise
   (ignores case, spaces and punctuation) */
int isPalindrome(const char *s)
{
    int i = 0, j = (int)strlen(s) - 1;

    while (i < j) {
        while (i < j && !isalnum((unsigned char)s[i])) i++;
        while (i < j && !isalnum((unsigned char)s[j])) j--;

        if (tolower((unsigned char)s[i]) != tolower((unsigned char)s[j]))
            return 0;
        i++;  j--;
    }
    return 1;
}

int main(void)
{
    int sockfd, connfd, opt = 1;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    char buff[BUFSZ], reply[BUFSZ + 64];
    int n;

    /* 1. create TCP socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) { perror("socket"); exit(1); }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 2. bind */
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;      /* any local interface */
    servaddr.sin_port        = htons(PORT);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind"); exit(1);
    }

    /* 3. listen */
    if (listen(sockfd, 5) < 0) { perror("listen"); exit(1); }
    printf("Palindrome server listening on port %d ...\n", PORT);

    /* 4. serve clients one after another */
    for (;;) {
        len = sizeof(cliaddr);
        connfd = accept(sockfd, (struct sockaddr *)&cliaddr, &len);
        if (connfd < 0) { perror("accept"); continue; }

        printf("Client connected: %s:%d\n",
               inet_ntoa(cliaddr.sin_addr), ntohs(cliaddr.sin_port));

        /* keep talking to this client until it closes / sends "exit" */
        while ((n = read(connfd, buff, BUFSZ - 1)) > 0) {
            buff[n] = '\0';
            buff[strcspn(buff, "\r\n")] = '\0';   /* remove newline */

            if (strcmp(buff, "exit") == 0) break;

            printf("Received : \"%s\"\n", buff);

            if (isPalindrome(buff))
                sprintf(reply, "\"%s\" IS a Palindrome", buff);
            else
                sprintf(reply, "\"%s\" is NOT a Palindrome", buff);

            write(connfd, reply, strlen(reply) + 1);
        }

        printf("Client disconnected.\n\n");
        close(connfd);
    }

    close(sockfd);
    return 0;
}
