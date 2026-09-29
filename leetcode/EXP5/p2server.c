#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT      9090
#define BUFSZ     4096
#define LOGFILE   "server_log.txt"
#define STOREDIR  "server_files"
#define MAXATTEMP 3

/* ------------------- credential database ------------------- */
struct cred { const char *user, *pass; };
static struct cred users[] = {
    {"alice", "alice123"},
    {"bob",   "bob456"  },
    {"admin", "admin@1" }
};
#define NUSERS (sizeof(users)/sizeof(users[0]))

static int authenticate(const char *u, const char *p)
{
    for (size_t i = 0; i < NUSERS; i++)
        if (!strcmp(users[i].user, u) && !strcmp(users[i].pass, p))
            return 1;
    return 0;
}

/* ------------------- logging ------------------- */
static void log_event(const char *ip, int port, const char *fmt, ...)
{
    FILE *fp = fopen(LOGFILE, "a");
    if (!fp) return;

    time_t t = time(NULL);
    char ts[64];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));

    char msg[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    fprintf(fp, "[%s] [%s:%d] %s\n", ts, ip ? ip : "-", port, msg);
    fclose(fp);
}

/* ------------------- socket helpers ------------------- */
static int send_all(int fd, const void *buf, size_t n)
{
    const char *p = buf; size_t left = n;
    while (left) {
        ssize_t s = send(fd, p, left, 0);
        if (s <= 0) return -1;
        p += s; left -= s;
    }
    return 0;
}

static int recv_all(int fd, void *buf, size_t n)
{
    char *p = buf; size_t left = n;
    while (left) {
        ssize_t r = recv(fd, p, left, 0);
        if (r <= 0) return -1;
        p += r; left -= r;
    }
    return 0;
}

/* read one '\n' terminated line ; 1=ok 0=peer closed -1=error */
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

static int send_line(int fd, const char *fmt, ...)
{
    char line[BUFSZ];
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(line, sizeof line - 2, fmt, ap);
    va_end(ap);
    line[n++] = '\n'; line[n] = '\0';
    return send_all(fd, line, n);
}

/* ------------------- services ------------------- */
static void svc_time(int fd)
{
    time_t t = time(NULL);
    char ts[128];
    strftime(ts, sizeof ts, "%A, %d %B %Y  %H:%M:%S", localtime(&t));
    send_line(fd, "OK Server Date & Time : %s", ts);
}

static void svc_sysinfo(int fd)
{
    struct utsname u;  struct sysinfo si;
    if (uname(&u) < 0) { send_line(fd, "ERR uname() failed"); return; }
    sysinfo(&si);
    send_line(fd,
      "OK OS=%s | Node=%s | Release=%s | Version=%s | Machine=%s | "
      "CPUs=%d | Uptime=%ld min | FreeRAM=%lu MB",
      u.sysname, u.nodename, u.release, u.version, u.machine,
      get_nprocs(), si.uptime/60, (unsigned long)(si.freeram/(1024*1024)));
}

/* client -> server  : UPLOAD <name> */
static void svc_upload(int fd, char *fname, const char *ip, int port)
{
    char path[512], line[BUFSZ], buf[BUFSZ];
    long size = 0;

    if (!fname || !*fname) { send_line(fd, "ERR Filename missing"); return; }
    if (strchr(fname, '/')) { send_line(fd, "ERR Invalid filename"); return; }

    mkdir(STOREDIR, 0755);
    snprintf(path, sizeof path, "%s/%s", STOREDIR, fname);

    FILE *fp = fopen(path, "wb");
    if (!fp) { send_line(fd, "ERR Cannot create file on server"); return; }

    send_line(fd, "READY");                       /* tell client to send */

    if (recv_line(fd, line, sizeof line) <= 0 ||
        sscanf(line, "SIZE %ld", &size) != 1 || size < 0) {
        fclose(fp); remove(path);
        send_line(fd, "ERR Bad size header");
        return;
    }

    long left = size;
    while (left > 0) {
        size_t chunk = (left > BUFSZ) ? BUFSZ : (size_t)left;
        if (recv_all(fd, buf, chunk) < 0) {
            fclose(fp); remove(path);
            log_event(ip, port, "UPLOAD %s FAILED (connection lost)", fname);
            return;
        }
        fwrite(buf, 1, chunk, fp);
        left -= chunk;
    }
    fclose(fp);
    send_line(fd, "OK Upload complete (%ld bytes stored as %s)", size, path);
    log_event(ip, port, "UPLOAD  '%s' (%ld bytes) - SUCCESS", fname, size);
}

/* client -> server  : DOWNLOAD <name> */
static void svc_download(int fd, char *fname, const char *ip, int port)
{
    char path[512], buf[BUFSZ];

    if (!fname || !*fname) { send_line(fd, "ERR Filename missing"); return; }
    snprintf(path, sizeof path, "%s/%s", STOREDIR, fname);

    FILE *fp = fopen(path, "rb");
    if (!fp) {
        send_line(fd, "ERR File '%s' not found on server", fname);
        log_event(ip, port, "DOWNLOAD '%s' - FAILED (not found)", fname);
        return;
    }
    fseek(fp, 0, SEEK_END); long size = ftell(fp); rewind(fp);

    send_line(fd, "SIZE %ld", size);
    size_t n;
    while ((n = fread(buf, 1, BUFSZ, fp)) > 0)
        if (send_all(fd, buf, n) < 0) break;
    fclose(fp);
    log_event(ip, port, "DOWNLOAD '%s' (%ld bytes) - SUCCESS", fname, size);
}

/* ------------------- per-client handler ------------------- */
static void handle_client(int fd, const char *ip, int port)
{
    char line[BUFSZ], cmd[64], arg[512], user[64], pass[64];
    int  logged = 0, attempts = 0;

    log_event(ip, port, "CONNECTED");

    /* ---------- authentication phase ---------- */
    while (!logged && attempts < MAXATTEMP) {
        if (recv_line(fd, line, sizeof line) <= 0) {
            log_event(ip, port, "DISCONNECTED during login"); close(fd); return; }

        if (sscanf(line, "LOGIN %63s %63s", user, pass) == 2 &&
            authenticate(user, pass)) {
            logged = 1;
            send_line(fd, "OK Welcome %s ! Your address is %s:%d",
                      user, ip, port);
            printf("[+] Login SUCCESS  user='%s'  from %s:%d\n", user, ip, port);
            log_event(ip, port, "LOGIN SUCCESS user='%s'", user);
        } else {
            attempts++;
            send_line(fd, "ERR Invalid credentials (attempt %d/%d)",
                      attempts, MAXATTEMP);
            printf("[-] Login FAILED   from %s:%d (attempt %d)\n",
                   ip, port, attempts);
            log_event(ip, port, "LOGIN FAILED (attempt %d)", attempts);
        }
    }
    if (!logged) {
        send_line(fd, "ERR Too many failed attempts. Closing connection.");
        log_event(ip, port, "CONNECTION CLOSED - authentication failure");
        close(fd); return;
    }

    /* ---------- service phase ---------- */
    while (1) {
        int r = recv_line(fd, line, sizeof line);
        if (r <= 0) { log_event(ip, port, "DISCONNECTED abruptly"); break; }

        cmd[0] = arg[0] = '\0';
        sscanf(line, "%63s %511[^\n]", cmd, arg);
        printf("    <%s:%d> request : %s\n", ip, port, line);

        if      (!strcasecmp(cmd, "TIME"))     { svc_time(fd);
                                                 log_event(ip,port,"DATETIME request"); }
        else if (!strcasecmp(cmd, "SYSINFO"))  { svc_sysinfo(fd);
                                                 log_event(ip,port,"SYSINFO request"); }
        else if (!strcasecmp(cmd, "UPLOAD"))     svc_upload(fd, arg, ip, port);
        else if (!strcasecmp(cmd, "DOWNLOAD"))   svc_download(fd, arg, ip, port);
        else if (!strcasecmp(cmd, "QUIT")) {
            send_line(fd, "OK Bye");
            log_event(ip, port, "LOGOUT - session terminated by client");
            break;
        }
        else {
            send_line(fd, "ERR Invalid request '%s'", cmd);
            log_event(ip, port, "INVALID request '%s'", line);
        }
    }
    close(fd);
    printf("[*] Session closed : %s:%d\n", ip, port);
    log_event(ip, port, "CONNECTION CLOSED");
}

/* ------------------- main ------------------- */
int main(void)
{
    int listenfd, connfd, opt = 1;
    struct sockaddr_in serv, cli;
    socklen_t clen;

    signal(SIGCHLD, SIG_IGN);              /* reap children automatically */
    signal(SIGPIPE, SIG_IGN);              /* ignore broken pipe          */

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        { perror("socket"); exit(1); }
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    memset(&serv, 0, sizeof serv);
    serv.sin_family      = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port        = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&serv, sizeof serv) < 0)
        { perror("bind"); exit(1); }
    if (listen(listenfd, 10) < 0)
        { perror("listen"); exit(1); }

    mkdir(STOREDIR, 0755);
    printf("=== Multi-Service TCP Server running on port %d ===\n", PORT);
    log_event("SERVER", 0, "Server started on port %d", PORT);

    while (1) {
        clen  = sizeof cli;
        connfd = accept(listenfd, (struct sockaddr *)&cli, &clen);
        if (connfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof ip);
        int  port = ntohs(cli.sin_port);
        printf("\n[*] New connection from %s : %d\n", ip, port);

        if (fork() == 0) {                 /* child */
            close(listenfd);
            handle_client(connfd, ip, port);
            exit(0);
        }
        close(connfd);                     /* parent */
    }
    close(listenfd);
    return 0;
}
