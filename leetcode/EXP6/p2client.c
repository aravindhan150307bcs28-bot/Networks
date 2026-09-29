#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 2048
#define SERVER_IP "127.0.0.1"

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    char input[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n"); return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    if(inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n"); return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n"); return -1;
    }

    printf("Connected to Hospital Server. Type commands (QUIT to exit).\n");
    printf("Commands:\n");
    printf("  REGISTER|Name|Phone|YYYY-MM-DD\n");
    printf("  ADDSLOT|DocID|YYYY-MM-DD|HH:MM|HH:MM  (Admin)\n");
    printf("  BOOK|PatientID|DoctorID|SlotID\n");
    printf("  CANCEL|AppID|PatientID\n");
    printf("  SEARCH|DoctorID|[YYYY-MM-DD]\n");
    printf("  HISTORY|PatientID\n");

    while(1) {
        printf("\n> ");
        fflush(stdout);
        if(!fgets(input, BUFFER_SIZE, stdin)) break;
        input[strcspn(input, "\n")] = 0; // Remove newline
        if(strlen(input) == 0) continue;
        if(strcmp(input, "QUIT") == 0) { send(sock, "QUIT\n", 5, 0); break; }

        strcat(input, "\n"); // Protocol requires newline
        send(sock, input, strlen(input), 0);

        memset(buffer, 0, BUFFER_SIZE);
        int valread = read(sock, buffer, BUFFER_SIZE - 1);
        if(valread <= 0) { printf("Server disconnected.\n"); break; }
        buffer[valread] = '\0';
        printf("Server: %s", buffer);
    }
    close(sock);
    return 0;
}
