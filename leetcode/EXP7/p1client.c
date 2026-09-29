#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>

#define SERVER_IP "127.0.0.1"
#define PORT 8080
#define BUFFER_SIZE 8192

// Reliable send (same as server)
ssize_t send_all(int sock, const void *buf, size_t len) {
    size_t total = 0; const char *p = buf;
    while (total < len) {
        ssize_t s = send(sock, p + total, len - total, 0);
        if (s <= 0) return -1; total += s;
    }
    return total;
}

// Read exactly 'len' bytes (for binary file data)
ssize_t recv_exact(int sock, void *buf, size_t len) {
    size_t total = 0; char *p = buf;
    while (total < len) {
        ssize_t r = recv(sock, p + total, len - total, 0);
        if (r == 0) return total; // EOF
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        total += r;
    }
    return total;
}

// Read one line (for text headers)
ssize_t recv_line(int sock, char *buf, size_t max) {
    size_t i=0; char c;
    while(i < max-1) {
        ssize_t r = recv(sock, &c, 1, 0);
        if(r==1) { if(c=='\n') break; if(c!='\r') buf[i++]=c; }
        else if(r==0) { if(i==0) return 0; break; }
        else { if(errno==EINTR) continue; return -1; }
    }
    buf[i]='\0'; return i;
}

void download_file(int sock, const char *filename) {
    char buffer[BUFFER_SIZE];
    char local_filename[256];
    long long filesize = 0;
    int fd = -1;
    ssize_t received;

    // Extract just the filename (strip path if server sent full path, though ours doesn't)
    snprintf(local_filename, sizeof(local_filename), "downloaded_%s", filename);

    // 1. Read Header: "OK <size>" or "ERROR ..."
    if (recv_line(sock, buffer, sizeof(buffer)) <= 0) { printf("Server disconnected.\n"); return; }

    if (strncmp(buffer, "OK ", 3) == 0) {
        filesize = atoll(buffer + 3);
        printf("[+] File exists. Size: %lld bytes. Saving as '%s'\n", filesize, local_filename);
    } else {
        printf("[-] Server: %s\n", buffer);
        return;
    }

    // 2. Open Local File
    fd = open(local_filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open local file"); return; }

    // 3. Receive Binary Data Loop
    long long total_received = 0;
    while (total_received < filesize) {
        size_t to_read = (filesize - total_received > BUFFER_SIZE) ? BUFFER_SIZE : (filesize - total_received);
        received = recv_exact(sock, buffer, to_read);
        if (received <= 0) { printf("\n[-] Connection lost or error during transfer.\n"); break; }

        if (write(fd, buffer, received) != received) { perror("write"); break; }

        total_received += received;
        // Progress Bar
        printf("\r[+] Progress: %.2f%% (%lld/%lld) ", (float)total_received/filesize*100, total_received, filesize);
        fflush(stdout);
    }

    close(fd);
    if (total_received == filesize) printf("\n[+] Download Complete!\n");
    else { unlink(local_filename); printf("\n[-] Download Failed/Incomplete. Removed partial file.\n"); }
}

void list_files(int sock) {
    char buffer[BUFFER_SIZE];
    int count = 0;

    if (recv_line(sock, buffer, sizeof(buffer)) <= 0) return;
    if (strncmp(buffer, "OK ", 3) == 0) count = atoi(buffer + 3);
    else { printf("[-] Server: %s\n", buffer); return; }

    printf("\n--- Available Files (%d) ---\n", count);
    while (1) {
        if (recv_line(sock, buffer, sizeof(buffer)) <= 0) break;
        if (strcmp(buffer, "END_LIST") == 0) break;
        printf("  %s\n", buffer);
    }
    printf("---------------------------\n");
}

int main() {
    int sock; struct sockaddr_in serv_addr; char input[BUFFER_SIZE]; char *cmd, *arg;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); return 1; }
    serv_addr.sin_family = AF_INET; serv_addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) { perror("inet_pton"); return 1; }
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { perror("connect"); return 1; }

    printf("Connected to File Server.\nCommands: LIST, GET <filename>, QUIT\n");

    while (1) {
        printf("\n> "); fflush(stdout);
        if (!fgets(input, sizeof(input), stdin)) break;
        input[strcspn(input, "\n")] = 0; // Trim newline
        if (strlen(input) == 0) continue;

        cmd = strtok(input, " ");
        arg = strtok(NULL, ""); // Rest of line

        if (strcmp(cmd, "LIST") == 0) {
            send_all(sock, "LIST\n", 5);
            list_files(sock);
        }
        else if (strcmp(cmd, "GET") == 0) {
            if (!arg) { printf("Usage: GET <filename>\n"); continue; }
            char req[BUFFER_SIZE];
            snprintf(req, sizeof(req), "GET %s\n", arg);
            send_all(sock, req, strlen(req));
            download_file(sock, arg);
        }
        else if (strcmp(cmd, "QUIT") == 0) {
            send_all(sock, "QUIT\n", 5);
            break;
        }
        else {
            printf("Unknown command.\n");
        }
    }
    close(sock);
    return 0;
}
