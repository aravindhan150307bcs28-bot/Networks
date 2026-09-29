#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT   6060
#define BUFSZ  2048

static int send_all(int fd, const char *b, size_t n)
{ while (n) { ssize_t s = send(fd, b, n, 0); if (s <= 0) return -1; b += s; n -= s; } return 0; }

static int send_line(int fd, const char *fmt, ...)
{
    char l[BUFSZ]; va_list ap; va_start(ap, fmt);
    int n = vsnprintf(l, sizeof l - 2, fmt, ap); va_end(ap);
    l[n++] = '\n'; l[n] = '\0';
    return send_all(fd, l, n);
}

static int recv_line(int fd, char *buf, size_t max)
{
    size_t i = 0; char c;
    while (i < max - 1) {
        ssize_t r = recv(fd, &c, 1, 0);
        if (r == 0) return 0;
        if (r <  0) return -1;
        if (c == '\n') break;
        buf[i++] = c;
    }
    buf[i] = '\0';
    if (i && buf[i-1] == '\r') buf[i-1] = '\0';
    return 1;
}

static void trim(char *s) { s[strcspn(s, "\r\n")] = '\0'; }

/* print every line until "END" */
static int recv_block(int fd)
{
    char line[BUFSZ];
    while (1) {
        if (recv_line(fd, line, sizeof line) <= 0) return -1;
        if (!strcmp(line, "END")) break;
        printf("%s\n", line);
    }
    return 0;
}

static int recv_one(int fd)
{
    char line[BUFSZ];
    if (recv_line(fd, line, sizeof line) <= 0) { printf("Server closed connection.\n"); return -1; }
    printf("Server: %s\n", line);
    return 0;
}

int main(int argc, char *argv[])
{
    int fd; struct sockaddr_in serv;
    char choice[16], line[BUFSZ];
    const char *ip = (argc > 1) ? argv[1] : "127.0.0.1";

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); exit(1); }

    memset(&serv, 0, sizeof serv);
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(PORT);
    if (inet_pton(AF_INET, ip, &serv.sin_addr) <= 0) { perror("address"); exit(1); }

    printf("Connecting to %s:%d ... (may wait if server is busy with another client)\n", ip, PORT);
    if (connect(fd, (struct sockaddr *)&serv, sizeof serv) < 0) { perror("connect"); exit(1); }

    if (recv_line(fd, line, sizeof line) > 0) printf("Server: %s\n", line);

    while (1) {
        printf("\n========= RAILWAY RESERVATION =========\n");
        printf(" 1. Display all trains\n");
        printf(" 2. Search trains (source -> destination)\n");
        printf(" 3. Check seat availability\n");
        printf(" 4. Book ticket\n");
        printf(" 5. Cancel ticket\n");
        printf(" 6. Booking status (PNR enquiry)\n");
        printf(" 7. Exit\n");
        printf("Enter choice : ");
        if (!fgets(choice, sizeof choice, stdin)) break;
        trim(choice);

        if (!strcmp(choice, "1")) {
            send_line(fd, "TRAINS");
            if (recv_block(fd) < 0) break;
        }
        else if (!strcmp(choice, "2")) {
            char s[32], d[32];
            printf("Source      : "); if (!fgets(s, sizeof s, stdin)) break; trim(s);
            printf("Destination : "); if (!fgets(d, sizeof d, stdin)) break; trim(d);
            if (!*s || !*d) { printf(" !! Both fields are required\n"); continue; }
            send_line(fd, "SEARCH %s %s", s, d);
            if (recv_block(fd) < 0) break;
        }
        else if (!strcmp(choice, "3")) {
            char t[16];
            printf("Train number : "); if (!fgets(t, sizeof t, stdin)) break; trim(t);
            send_line(fd, "AVAIL %s", t);
            if (recv_one(fd) < 0) break;
        }
        else if (!strcmp(choice, "4")) {
            char t[16], nm[40], ag[8], st[8];
            printf("Train number     : "); if (!fgets(t , sizeof t , stdin)) break; trim(t);
            printf("Passenger name   : "); if (!fgets(nm, sizeof nm, stdin)) break; trim(nm);
            printf("Age              : "); if (!fgets(ag, sizeof ag, stdin)) break; trim(ag);
            printf("Number of seats  : "); if (!fgets(st, sizeof st, stdin)) break; trim(st);
            if (!*t || !*nm || !*ag || !*st) { printf(" !! All fields are required\n"); continue; }
            for (char *p = nm; *p; p++) if (*p == ' ') *p = '_';   /* single token */
            send_line(fd, "BOOK %s %s %s %s", t, nm, ag, st);
            if (recv_one(fd) < 0) break;
        }
        else if (!strcmp(choice, "5")) {
            char p[16];
            printf("PNR to cancel : "); if (!fgets(p, sizeof p, stdin)) break; trim(p);
            send_line(fd, "CANCEL %s", p);
            if (recv_one(fd) < 0) break;
        }
        else if (!strcmp(choice, "6")) {
            char p[16];
            printf("Enter PNR : "); if (!fgets(p, sizeof p, stdin)) break; trim(p);
            send_line(fd, "STATUS %s", p);
            if (recv_one(fd) < 0) break;
        }
        else if (!strcmp(choice, "7")) {
            send_line(fd, "QUIT");
            recv_one(fd);
            printf("Disconnected.\n");
            break;
        }
        else printf(" !! Invalid choice, enter 1-7\n");
    }
    close(fd);
    return 0;
}
