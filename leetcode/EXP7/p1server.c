#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h> // PATH_MAX
#include <libgen.h> // basename (optional, for logging)

#define PORT 8080
#define BACKLOG 10
#define BUFFER_SIZE 8192      // 8KB chunks for file transfer
#define ROOT_DIR "./server_files" // Directory to serve files from
#define MAX_PATH 4096

// ==========================================
// UTILITY: Reliable I/O Helpers
// ==========================================

// Send exactly 'len' bytes (handles partial writes)
ssize_t send_all(int sock, const void *buf, size_t len) {
    size_t total = 0;
    const char *ptr = (const char *)buf;
    while (total < len) {
        ssize_t sent = send(sock, ptr + total, len - total, 0);
        if (sent <= 0) {
            if (errno == EINTR) continue;
            return -1; // Error or connection closed
        }
        total += sent;
    }
    return total;
}

// Receive exactly one line (ending in \n or EOF)
// Returns length of line (excluding \n), -1 on error, 0 on EOF/Connection Close
ssize_t recv_line(int sock, char *buf, size_t max_len) {
    size_t i = 0;
    char c;
    while (i < max_len - 1) {
        ssize_t r = recv(sock, &c, 1, 0);
        if (r == 1) {
            if (c == '\n') break;
            if (c != '\r') buf[i++] = c; // Ignore CR
        } else if (r == 0) { // EOF
            if (i == 0) return 0;
            break; // Partial line at EOF
        } else { // Error
            if (errno == EINTR) continue;
            return -1;
        }
    }
    buf[i] = '\0';
    return i;
}

// ==========================================
// SECURITY: Path Validation
// ==========================================
// Resolves requested path relative to ROOT_DIR, ensures it stays inside ROOT_DIR.
int validate_path(const char *requested_file, char *safe_path, size_t max_len) {
    char requested_full[MAX_PATH];
    char root_real[MAX_PATH];
    char requested_real[MAX_PATH];

    // 1. Get Absolute Path of Root
    if (realpath(ROOT_DIR, root_real) == NULL) return -1; // Root missing

    // 2. Construct Requested Path (ROOT_DIR + requested_file)
    // Prevent absolute paths from client
    if (requested_file[0] == '/') return -1;
    snprintf(requested_full, sizeof(requested_full), "%s/%s", ROOT_DIR, requested_file);

    // 3. Resolve Symlinks/.. in Requested Path
    if (realpath(requested_full, requested_real) == NULL) return -1; // File doesn't exist

    // 4. Security Check: Ensure requested_real starts with root_real
    if (strncmp(requested_real, root_real, strlen(root_real)) != 0) return -1; // Traversal attempt

    // 5. Ensure it's a regular file (not dir, device, etc)
    struct stat st;
    if (stat(requested_real, &st) != 0 || !S_ISREG(st.st_mode)) return -1;

    strncpy(safe_path, requested_real, max_len);
    safe_path[max_len - 1] = '\0';
    return 0;
}

// ==========================================
// COMMAND HANDLERS
// ==========================================

void handle_list(int client_sock) {
    DIR *d;
    struct dirent *dir;
    struct stat st;
    char full_path[MAX_PATH];
    char response[BUFFER_SIZE];
    int file_count = 0;
    long long total_size = 0; // Not strictly needed but good for header

    // First pass: Count files to send header
    // (Alternatively, stream directly, but header helps client UI)
    // We'll build a temporary buffer for the list body.
    char list_body[BUFFER_SIZE * 4] = {0}; // Large buffer for list
    size_t body_len = 0;

    d = opendir(ROOT_DIR);
    if (!d) {
        send_all(client_sock, "ERROR Cannot open server directory\n", 35);
        return;
    }

    while ((dir = readdir(d)) != NULL) {
        // Skip hidden files and directories
        if (dir->d_name[0] == '.') continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", ROOT_DIR, dir->d_name);
        if (stat(full_path, &st) == 0 && S_ISREG(st.st_mode)) {
            file_count++;
            int written = snprintf(list_body + body_len, sizeof(list_body) - body_len,
                                   "%s %lld\n", dir->d_name, (long long)st.st_size);
            if (written > 0 && (size_t)written < sizeof(list_body) - body_len) body_len += written;
        }
    }
    closedir(d);

    // Send Header
    int header_len = snprintf(response, sizeof(response), "OK %d\n", file_count);
    send_all(client_sock, response, header_len);

    // Send Body
    if (body_len > 0) send_all(client_sock, list_body, body_len);

    // Send Terminator
    send_all(client_sock, "END_LIST\n", 9);
}

void handle_get(int client_sock, const char *filename) {
    char safe_path[MAX_PATH];
    int fd = -1;
    struct stat st;
    char header[128];
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // 1. Validate Path (Security Critical)
    if (validate_path(filename, safe_path, sizeof(safe_path)) != 0) {
        send_all(client_sock, "ERROR File not found or access denied\n", 39);
        return;
    }

    // 2. Open File
    fd = open(safe_path, O_RDONLY);
    if (fd < 0) {
        send_all(client_sock, "ERROR Cannot open file\n", 23);
        return;
    }

    // 3. Get Size
    if (fstat(fd, &st) < 0) {
        close(fd);
        send_all(client_sock, "ERROR File stat failed\n", 23);
        return;
    }

    // 4. Send Header: OK <filesize>\n
    int header_len = snprintf(header, sizeof(header), "OK %lld\n", (long long)st.st_size);
    if (send_all(client_sock, header, header_len) < 0) {
        close(fd);
        return; // Client disconnected
    }

    // 5. Stream File Content (Binary)
    while ((bytes_read = read(fd, buffer, BUFFER_SIZE)) > 0) {
        if (send_all(client_sock, buffer, bytes_read) < 0) {
            break; // Client disconnected or error
        }
    }

    close(fd);
    // No terminator needed for binary stream; client reads exactly 'filesize' bytes.
}

// ==========================================
// THREAD WORKER FUNCTION
// ==========================================
void *client_handler(void *arg) {
    int client_sock = *(int *)arg;
    free(arg); // Free the malloc'd socket int
    char command[BUFFER_SIZE];
    char *cmd, *arg_ptr;
    struct sockaddr_in peer_addr;
    socklen_t peer_len = sizeof(peer_addr);
    getpeername(client_sock, (struct sockaddr*)&peer_addr, &peer_len);
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &peer_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

    printf("[+] Thread started for %s:%d\n", client_ip, ntohs(peer_addr.sin_port));

    while (1) {
        ssize_t len = recv_line(client_sock, command, sizeof(command));
        if (len <= 0) { // 0 = EOF (Client closed), -1 = Error
            printf("[-] Client %s disconnected (recv: %zd).\n", client_ip, len);
            break;
        }

        // Parse Command
        cmd = strtok(command, " \t");
        arg_ptr = strtok(NULL, " \t"); // Filename for GET

        if (!cmd) continue;

        if (strcmp(cmd, "LIST") == 0) {
            printf("[%s] CMD: LIST\n", client_ip);
            handle_list(client_sock);
        }
        else if (strcmp(cmd, "GET") == 0) {
            if (!arg_ptr) {
                send_all(client_sock, "ERROR Usage: GET <filename>\n", 29);
            } else {
                printf("[%s] CMD: GET %s\n", client_ip, arg_ptr);
                handle_get(client_sock, arg_ptr);
            }
        }
        else if (strcmp(cmd, "QUIT") == 0) {
            printf("[%s] CMD: QUIT\n", client_ip);
            send_all(client_sock, "OK Goodbye\n", 11);
            break;
        }
        else {
            send_all(client_sock, "ERROR Unknown command\n", 22);
        }
    }

    close(client_sock);
    printf("[+] Thread finished for %s\n", client_ip);
    return NULL;
}

// ==========================================
// MAIN SERVER LOOP
// ==========================================
int main() {
    int server_fd, *client_sock_ptr;
    struct sockaddr_in address, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    pthread_t thread_id;

    // 1. Setup Root Directory
    struct stat st = {0};
    if (stat(ROOT_DIR, &st) == -1) {
        mkdir(ROOT_DIR, 0700);
        printf("[*] Created root directory: %s\n", ROOT_DIR);
        printf("[*] Place files to share inside '%s/'\n", ROOT_DIR);
    }

    // 2. Socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed"); exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // 3. Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed"); close(server_fd); exit(EXIT_FAILURE);
    }

    // 4. Listen
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen"); close(server_fd); exit(EXIT_FAILURE);
    }

    printf("=== File Sharing Server Started on Port %d ===\n", PORT);
    printf("=== Serving files from: %s ===\n", ROOT_DIR);
    printf("=== Waiting for connections... ===\n\n");

    // 5. Accept Loop
    while (1) {
        // Allocate memory for socket descriptor to pass to thread safely
        client_sock_ptr = malloc(sizeof(int));
        if (!client_sock_ptr) { perror("malloc"); continue; }

        *client_sock_ptr = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (*client_sock_ptr < 0) {
            perror("accept");
            free(client_sock_ptr);
            continue;
        }

        // Create Detached Thread
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED); // Auto cleanup

        if (pthread_create(&thread_id, &attr, client_handler, (void*)client_sock_ptr) != 0) {
            perror("pthread_create");
            close(*client_sock_ptr);
            free(client_sock_ptr);
        }
        pthread_attr_destroy(&attr);
    }

    close(server_fd);
    return 0;
}
