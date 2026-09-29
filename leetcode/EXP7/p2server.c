
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT     9001
#define BUFSZ    4096
#define LOGFILE  "server_log.txt"
#define FILEDIR  "server_files"

/* ---------------- user database ---------------- */
typedef struct { char user[32], pass[32]; } Cred;
static Cred users[] = { {"admin","admin123"},
                        {"alice","alice123"},
                        {"bob",  "bob123"  } };
static const int NUSERS = 3;

/* ---------------- logging ---------------- */
static void logmsg(const char *ip, int port, const char *user,
                   const char *fmt, ...)
{
    FILE *f = fopen(LOGFILE, "a");
    if (!f) return;
    time_t t = time(NULL);
    char ts[64];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
    fprintf(f, "[%s] %s:%d user=%-8s | ", ts, ip, port,
            (user && *user) ? user : "-");
    va_list ap; va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fputc('\n', f);
    fclose(f);
}

/* ---------------- socket I/O helpers ---------------- */
static int send_all(int s, const void *buf, size_t n)
{
    const char *p = buf;
    while (n) { ssize_t k = send(s, p, n, 0); if (k <= 0) return -1; p += k; n -= k; }
    return 0;
}
static int recv_all(int s, void *buf, size_t n)
{
    char *p = buf;
    while (n) { ssize_t k = recv(s, p, n, 0); if (k <= 0) return -1; p += k; n -= k; }
    return 0;
}
static int recv_line(int s, char *buf, int max)
{
    int i = 0; char c;
    while (i < max - 1) {
        ssize_t k = recv(s, &c, 1, 0);
        if (k <= 0) return -1;
        if (c == '\n') break;
        if (c != '\r') buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}
static int send_line(int s, const char *fmt, ...)
{
    char t[BUFSZ]; va_list ap; va_start(ap, fmt);
    vsnprintf(t, sizeof t - 2, fmt, ap); va_end(ap);
    strcat(t, "\n");
    return send_all(s, t, strlen(t));
}

/* ---------------- services ---------------- */
static const char *base_name(const char *p)
{
    const char *s = strrchr(p, '/');
    return s ? s + 1 : p;
}

static void svc_time(int c)
{
    time_t t = time(NULL);
    char ts[80];
    strftime(ts, sizeof ts, "%A, %d %B %Y  %H:%M:%S", localtime(&t));
    send_line(c, "OK Server date & time: %s", ts);
}

static void svc_sysinfo(int c)
{
    struct utsname u;
    char host[128] = "unknown";
    uname(&u);
    gethostname(host, sizeof host);
    send_line(c, "OK ---- SERVER SYSTEM INFORMATION ----");
    send_line(c, "Hostname        : %s", host);
    send_line(c, "OS Name         : %s", u.sysname);
    send_line(c, "Node name       : %s", u.nodename);
    send_line(c, "Kernel Release  : %s", u.release);
    send_line(c, "Kernel Version  : %s", u.version);
    send_line(c, "Machine/Arch    : %s", u.machine);
    send_line(c, "CPU cores       : %ld", sysconf(_SC_NPROCESSORS_ONLN));
    send_line(c, "Page size       : %ld bytes", sysconf(_SC_PAGESIZE));
    send_line(c, "Server PID      : %d", (int)getpid());
    send_line(c, "END");
}

static void svc_upload(int c, char *args, const char *ip, int port, const char *usr)
{
    char fname[256]; long size;
    if (sscanf(args, "%255s %ld", fname, &size) != 2 || size < 0) {
        send_line(c, "ERR Usage: UPLOAD <filename> <size>");
        return;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/%s", FILEDIR, base_name(fname));
    FILE *fp = fopen(path, "wb");
    if (!fp) { send_line(c, "ERR Cannot create file on server"); return; }

    send_line(c, "READY");

    char buf[BUFSZ]; long left = size; int ok = 1;
    while (left > 0) {
        size_t chunk = (left > BUFSZ) ? BUFSZ : (size_t)left;
        if (recv_all(c, buf, chunk) < 0) { ok = 0; break; }
        fwrite(buf, 1, chunk, fp);
        left -= chunk;
    }
    fclose(fp);
    if (ok) {
        send_line(c, "OK Stored '%s' (%ld bytes) on server", base_name(fname), size);
        logmsg(ip, port, usr, "UPLOAD  '%s' %ld bytes -> SUCCESS", base_name(fname), size);
    } else {
        remove(path);
        logmsg(ip, port, usr, "UPLOAD  '%s' -> FAILED (connection lost)", fname);
    }
}

static void svc_download(int c, char *args, const char *ip, int port, const char *usr)
{
    char fname[256];
    if (sscanf(args, "%255s", fname) != 1) {
        send_line(c, "ERR Usage: DOWNLOAD <filename>"); return;
    }
    char path[512];
    snprintf(path, sizeof path, "%s/%s", FILEDIR, base_name(fname));
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        send_line(c, "ERR File '%s' not found on server", base_name(fname));
        logmsg(ip, port, usr, "DOWNLOAD '%s' -> NOT FOUND", fname);
        return;
    }
    struct stat st; stat(path, &st);
    send_line(c, "OK %ld", (long)st.st_size);

    char buf[BUFSZ]; size_t n;
    while ((n = fread(buf, 1, BUFSZ, fp)) > 0)
        if (send_all(c, buf, n) < 0) break;
    fclose(fp);
    logmsg(ip, port, usr, "DOWNLOAD '%s' %ld bytes -> SUCCESS",
           base_name(fname), (long)st.st_size);
}

/* ---------------- per-client handler ---------------- */
static void handle_client(int c, const char *ip, int port)
{
    char line[BUFSZ], cmd[32], usr[32] = "";
    int authenticated = 0, attempts = 0;

    send_line(c, "WELCOME Secure Multi-Service Server. Please LOGIN.");
    logmsg(ip, port, "-", "CONNECTED");

    /* ---- authentication phase (3 attempts) ---- */
    while (!authenticated && attempts < 3) {
        if (recv_line(c, line, sizeof line) < 0) { close(c); return; }
        char u[32], p[32];
        if (sscanf(line, "%31s %31s %31s", cmd, u, p) == 3 &&
            strcasecmp(cmd, "LOGIN") == 0) {
            int found = 0;
            for (int i = 0; i < NUSERS; i++)
                if (!strcmp(users[i].user, u) && !strcmp(users[i].pass, p)) { found = 1; break; }
            attempts++;
            if (found) {
                authenticated = 1;
                strcpy(usr, u);
                send_line(c, "OK Login successful. Welcome %s! "
                             "(your address %s:%d)", u, ip, port);
                printf(">> LOGIN SUCCESS  user='%s'  client=%s:%d\n", u, ip, port);
                logmsg(ip, port, u, "LOGIN SUCCESS");
            } else {
                send_line(c, "ERR Invalid credentials (attempt %d of 3)", attempts);
                printf(">> LOGIN FAILED   user='%s'  client=%s:%d\n", u, ip, port);
                logmsg(ip, port, u, "LOGIN FAILED");
            }
        } else if (!strcasecmp(line, "QUIT")) {
            send_line(c, "OK Bye"); close(c); return;
        } else {
            send_line(c, "ERR Please login: LOGIN <username> <password>");
        }
    }
    if (!authenticated) {
        send_line(c, "ERR Too many failed attempts. Disconnecting.");
        logmsg(ip, port, "-", "DISCONNECTED (auth failure)");
        close(c); return;
    }

    /* ---- service phase ---- */
    for (;;) {
        if (recv_line(c, line, sizeof line) < 0) break;
        if (!line[0]) continue;

        cmd[0] = '\0';
        sscanf(line, "%31s", cmd);
        char *args = line + strlen(cmd);
        while (*args == ' ') args++;

        if (!strcasecmp(cmd, "UPLOAD"))        svc_upload(c, args, ip, port, usr);
        else if (!strcasecmp(cmd, "DOWNLOAD")) svc_download(c, args, ip, port, usr);
        else if (!strcasecmp(cmd, "TIME"))   { svc_time(c);    logmsg(ip,port,usr,"TIME request"); }
        else if (!strcasecmp(cmd, "SYSINFO")){ svc_sysinfo(c); logmsg(ip,port,usr,"SYSINFO request"); }
        else if (!strcasecmp(cmd, "QUIT"))   { send_line(c, "OK Bye"); break; }
        else {
            send_line(c, "ERR Invalid request '%s'. Valid: UPLOAD|DOWNLOAD|TIME|SYSINFO|QUIT", cmd);
            logmsg(ip, port, usr, "INVALID REQUEST '%s'", line);
        }
    }
    logmsg(ip, port, usr, "DISCONNECTED");
    printf(">> Client %s:%d disconnected\n", ip, port);
    close(c);
}

/* ---------------- main ---------------- */
int main(void)
{
    int sfd, cfd, opt = 1;
    struct sockaddr_in serv, cli;
    socklen_t len;

    mkdir(FILEDIR, 0777);
    signal(SIGCHLD, SIG_IGN);           /* auto-reap children */

    if ((sfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); exit(1); }
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    memset(&serv, 0, sizeof serv);
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = htons(PORT);

    if (bind(sfd, (struct sockaddr*)&serv, sizeof serv) < 0) { perror("bind"); exit(1); }
    if (listen(sfd, 10) < 0) { perror("listen"); exit(1); }

    printf("=== Multi-Service Authenticated Server on port %d ===\n", PORT);
    printf("Shared directory : ./%s   Log file : %s\n\n", FILEDIR, LOGFILE);

    for (;;) {
        len = sizeof cli;
        if ((cfd = accept(sfd, (struct sockaddr*)&cli, &len)) < 0) { perror("accept"); continue; }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof ip);
        int port = ntohs(cli.sin_port);
        printf(">> New connection from IP: %s  Port: %d\n", ip, port);

        if (fork() == 0) {              /* child serves the client */
            close(sfd);
            handle_client(cfd, ip, port);
            exit(0);
        }
        close(cfd);                     /* parent continues */
    }
    close(sfd);
    return 0;
}
