#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT       6060
#define BUFSZ      2048
#define TRAINFILE  "trains.dat"
#define BOOKFILE   "bookings.dat"
#define LOGFILE    "railway_log.txt"
#define MAXSEATS   6          /* max seats per booking */

/* ------------------------ records ------------------------ */
typedef struct {
    int   no;                 /* train number      */
    char  name[40];           /* train name        */
    char  src[20], dst[20];   /* source / dest     */
    char  dep[8],  arr[8];    /* timings           */
    int   total, avail;       /* seats             */
    float fare;               /* fare per seat     */
} Train;

typedef struct {
    long  pnr;
    int   trainno;
    char  tname[40];
    char  pname[40];
    int   age;
    int   seats;
    float amount;
    char  status[12];         /* CONFIRMED / CANCELLED */
    char  bookedon[24];
} Booking;

/* ------------------------ logging ------------------------ */
static void log_event(const char *ip, int port, const char *fmt, ...)
{
    FILE *fp = fopen(LOGFILE, "a");
    if (!fp) return;
    time_t t = time(NULL);
    char ts[32];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));

    char msg[512];
    va_list ap; va_start(ap, fmt);
    vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);

    fprintf(fp, "[%s] [%s:%d] %s\n", ts, ip, port, msg);
    fclose(fp);
}

/* ------------------------ socket helpers ------------------------ */
static int send_all(int fd, const char *b, size_t n)
{ while (n) { ssize_t s = send(fd, b, n, 0); if (s <= 0) return -1; b += s; n -= s; } return 0; }

static int send_line(int fd, const char *fmt, ...)
{
    char l[BUFSZ]; va_list ap; va_start(ap, fmt);
    int n = vsnprintf(l, sizeof l - 2, fmt, ap); va_end(ap);
    l[n++] = '\n'; l[n] = '\0';
    return send_all(fd, l, n);
}

/* read one line : 1 = ok, 0 = peer closed, -1 = error */
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

/* ------------------------ database ------------------------ */
static void init_database(void)
{
    FILE *fp = fopen(TRAINFILE, "rb");
    if (fp) { fclose(fp); return; }            /* already exists */

    Train t[] = {
      {12951,"Rajdhani Express",   "MUMBAI", "DELHI",  "17:00","08:35",80,80,2550.0f},
      {12627,"Karnataka Express",  "BANGALORE","DELHI","19:20","06:40",60,60,1875.5f},
      {12839,"Howrah Mail",        "CHENNAI","HOWRAH", "22:45","04:50",70,70,1420.0f},
      {12002,"Shatabdi Express",   "DELHI", "BHOPAL",  "06:00","14:10",50,50, 980.0f},
      {16526,"Island Express",     "BANGALORE","KANYAKUMARI","21:00","13:30",40,40,760.0f},
      {12723,"Telangana Express",  "HYDERABAD","DELHI","06:05","07:10",65,65,1990.0f}
    };
    fp = fopen(TRAINFILE, "wb");
    if (!fp) { perror("create trains.dat"); exit(1); }
    fwrite(t, sizeof(Train), sizeof(t)/sizeof(t[0]), fp);
    fclose(fp);
    printf("[*] Train database created with %zu trains\n", sizeof(t)/sizeof(t[0]));
}

/* find a train ; returns index (>=0) and fills *out, else -1 */
static int find_train(int no, Train *out)
{
    FILE *fp = fopen(TRAINFILE, "rb");
    if (!fp) return -1;
    Train t; int idx = 0;
    while (fread(&t, sizeof t, 1, fp) == 1) {
        if (t.no == no) { if (out) *out = t; fclose(fp); return idx; }
        idx++;
    }
    fclose(fp);
    return -1;
}

/* update seat count of a train (delta may be -n or +n) */
static int update_seats(int no, int delta)
{
    FILE *fp = fopen(TRAINFILE, "r+b");
    if (!fp) return -1;
    Train t; long pos;
    while ((pos = ftell(fp)), fread(&t, sizeof t, 1, fp) == 1) {
        if (t.no == no) {
            if (t.avail + delta < 0 || t.avail + delta > t.total) { fclose(fp); return -1; }
            t.avail += delta;
            fseek(fp, pos, SEEK_SET);
            fwrite(&t, sizeof t, 1, fp);
            fflush(fp);
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

static long next_pnr(void)
{
    FILE *fp = fopen(BOOKFILE, "rb");
    long cnt = 0;
    if (fp) { fseek(fp, 0, SEEK_END); cnt = ftell(fp)/(long)sizeof(Booking); fclose(fp); }
    return 100001L + cnt;
}

static int find_booking(long pnr, Booking *out, long *pos)
{
    FILE *fp = fopen(BOOKFILE, "rb");
    if (!fp) return -1;
    Booking b; long p;
    while ((p = ftell(fp)), fread(&b, sizeof b, 1, fp) == 1) {
        if (b.pnr == pnr) { if (out) *out = b; if (pos) *pos = p; fclose(fp); return 0; }
    }
    fclose(fp);
    return -1;
}

/* ------------------------ services ------------------------ */
static void svc_list(int fd)
{
    FILE *fp = fopen(TRAINFILE, "rb");
    send_line(fd, "-------------------------------------------------------------------------------");
    send_line(fd, "TrNo   Train Name            From        To          Dep    Arr    Avail  Fare");
    send_line(fd, "-------------------------------------------------------------------------------");
    Train t;
    while (fp && fread(&t, sizeof t, 1, fp) == 1)
        send_line(fd, "%-6d %-21s %-11s %-11s %-6s %-6s %3d/%-3d %.2f",
                  t.no, t.name, t.src, t.dst, t.dep, t.arr, t.avail, t.total, t.fare);
    if (fp) fclose(fp);
    send_line(fd, "-------------------------------------------------------------------------------");
    send_line(fd, "END");
}

static void svc_search(int fd, const char *src, const char *dst)
{
    FILE *fp = fopen(TRAINFILE, "rb");
    Train t; int found = 0;
    send_line(fd, "Trains from %s to %s :", src, dst);
    send_line(fd, "-------------------------------------------------------------------------------");
    send_line(fd, "TrNo   Train Name            From        To          Dep    Arr    Avail  Fare");
    send_line(fd, "-------------------------------------------------------------------------------");
    while (fp && fread(&t, sizeof t, 1, fp) == 1) {
        if (!strcasecmp(t.src, src) && !strcasecmp(t.dst, dst)) {
            send_line(fd, "%-6d %-21s %-11s %-11s %-6s %-6s %3d/%-3d %.2f",
                      t.no, t.name, t.src, t.dst, t.dep, t.arr, t.avail, t.total, t.fare);
            found++;
        }
    }
    if (fp) fclose(fp);
    if (!found) send_line(fd, "  << No train available on this route >>");
    send_line(fd, "-------------------------------------------------------------------------------");
    send_line(fd, "END");
}

static void svc_avail(int fd, const char *arg, const char *ip, int port)
{
    int no;
    if (sscanf(arg, "%d", &no) != 1) {
        send_line(fd, "ERR Train number missing/invalid");
        log_event(ip, port, "AVAIL - invalid argument");
        return;
    }
    Train t;
    if (find_train(no, &t) < 0) {
        send_line(fd, "ERR Train %d does not exist", no);
        log_event(ip, port, "AVAIL %d - FAILED (no such train)", no);
        return;
    }
    send_line(fd, "OK Train %d (%s) %s->%s : %d of %d seats available | Fare Rs.%.2f",
              t.no, t.name, t.src, t.dst, t.avail, t.total, t.fare);
    log_event(ip, port, "AVAIL %d - avail=%d", no, t.avail);
}

static void svc_book(int fd, const char *arg, const char *ip, int port)
{
    int no, age, seats;
    char name[40];

    if (sscanf(arg, "%d %39s %d %d", &no, name, &age, &seats) != 4) {
        send_line(fd, "ERR Usage: BOOK <trainno> <name> <age> <seats>");
        log_event(ip, port, "BOOK - malformed request '%s'", arg);
        return;
    }
    if (age <= 0 || age > 120) {
        send_line(fd, "ERR Invalid age (1-120)");
        log_event(ip, port, "BOOK - invalid age %d", age);
        return;
    }
    if (seats <= 0 || seats > MAXSEATS) {
        send_line(fd, "ERR Seats must be between 1 and %d", MAXSEATS);
        log_event(ip, port, "BOOK - invalid seat count %d", seats);
        return;
    }
    Train t;
    if (find_train(no, &t) < 0) {
        send_line(fd, "ERR Train %d does not exist", no);
        log_event(ip, port, "BOOK %d - FAILED (no such train)", no);
        return;
    }
    if (t.avail < seats) {
        send_line(fd, "ERR Only %d seat(s) left in train %d - booking rejected",
                  t.avail, no);
        log_event(ip, port, "BOOK %d - FAILED (need %d, avail %d)", no, seats, t.avail);
        return;
    }

    /* ---- transaction ---- */
    if (update_seats(no, -seats) < 0) {
        send_line(fd, "ERR Database update failed");
        log_event(ip, port, "BOOK %d - DB ERROR", no);
        return;
    }
    Booking b;
    memset(&b, 0, sizeof b);
    b.pnr     = next_pnr();
    b.trainno = no;
    strncpy(b.tname, t.name, sizeof b.tname - 1);
    strncpy(b.pname, name,   sizeof b.pname - 1);
    b.age     = age;
    b.seats   = seats;
    b.amount  = seats * t.fare;
    strcpy(b.status, "CONFIRMED");
    time_t now = time(NULL);
    strftime(b.bookedon, sizeof b.bookedon, "%Y-%m-%d %H:%M:%S", localtime(&now));

    FILE *fp = fopen(BOOKFILE, "ab");
    if (!fp) { update_seats(no, seats);            /* rollback */
               send_line(fd, "ERR Cannot write booking record"); return; }
    fwrite(&b, sizeof b, 1, fp);
    fflush(fp); fclose(fp);

    send_line(fd, "OK BOOKING CONFIRMED | PNR=%ld | Train=%d (%s) | Passenger=%s(%d) | "
                  "Seats=%d | Amount=Rs.%.2f | On=%s",
              b.pnr, b.trainno, b.tname, b.pname, b.age, b.seats, b.amount, b.bookedon);
    log_event(ip, port, "BOOK SUCCESS PNR=%ld train=%d pax='%s' seats=%d amt=%.2f",
              b.pnr, no, name, seats, b.amount);
}

static void svc_cancel(int fd, const char *arg, const char *ip, int port)
{
    long pnr;
    if (sscanf(arg, "%ld", &pnr) != 1) {
        send_line(fd, "ERR PNR missing/invalid");
        log_event(ip, port, "CANCEL - invalid argument");
        return;
    }
    Booking b; long pos;
    if (find_booking(pnr, &b, &pos) < 0) {
        send_line(fd, "ERR PNR %ld not found", pnr);
        log_event(ip, port, "CANCEL %ld - FAILED (not found)", pnr);
        return;
    }
    if (!strcmp(b.status, "CANCELLED")) {
        send_line(fd, "ERR PNR %ld is already cancelled", pnr);
        log_event(ip, port, "CANCEL %ld - FAILED (already cancelled)", pnr);
        return;
    }
    strcpy(b.status, "CANCELLED");
    FILE *fp = fopen(BOOKFILE, "r+b");
    if (!fp) { send_line(fd, "ERR Database error"); return; }
    fseek(fp, pos, SEEK_SET);
    fwrite(&b, sizeof b, 1, fp);
    fflush(fp); fclose(fp);

    update_seats(b.trainno, b.seats);              /* release seats */

    send_line(fd, "OK PNR %ld CANCELLED | %d seat(s) released in train %d | "
                  "Refund Rs.%.2f", pnr, b.seats, b.trainno, b.amount * 0.90f);
    log_event(ip, port, "CANCEL SUCCESS PNR=%ld train=%d seats=%d",
              pnr, b.trainno, b.seats);
}

static void svc_status(int fd, const char *arg, const char *ip, int port)
{
    long pnr;
    if (sscanf(arg, "%ld", &pnr) != 1) {
        send_line(fd, "ERR PNR missing/invalid");
        log_event(ip, port, "STATUS - invalid argument");
        return;
    }
    Booking b;
    if (find_booking(pnr, &b, NULL) < 0) {
        send_line(fd, "ERR PNR %ld not found", pnr);
        log_event(ip, port, "STATUS %ld - FAILED (not found)", pnr);
        return;
    }
    send_line(fd, "OK PNR=%ld | Train=%d (%s) | Passenger=%s(%d) | Seats=%d | "
                  "Amount=Rs.%.2f | Status=%s | BookedOn=%s",
              b.pnr, b.trainno, b.tname, b.pname, b.age, b.seats,
              b.amount, b.status, b.bookedon);
    log_event(ip, port, "STATUS %ld - %s", pnr, b.status);
}

/* ------------------------ one client session ------------------------ */
static void serve_client(int fd, const char *ip, int port)
{
    char line[BUFSZ], cmd[32], arg[BUFSZ];

    send_line(fd, "OK Connected to Railway Reservation Server. Your address %s:%d", ip, port);
    log_event(ip, port, "CONNECTED");

    while (1) {
        int r = recv_line(fd, line, sizeof line);
        if (r <= 0) { log_event(ip, port, "DISCONNECTED abruptly"); break; }
        if (!line[0]) continue;

        cmd[0] = arg[0] = '\0';
        sscanf(line, "%31s %[^\n]", cmd, arg);
        printf("    <%s:%d> %s\n", ip, port, line);

        if (!strcasecmp(cmd, "TRAINS")) {
            svc_list(fd);                       log_event(ip, port, "LIST TRAINS");
        }
        else if (!strcasecmp(cmd, "SEARCH")) {
            char s[20], d[20];
            if (sscanf(arg, "%19s %19s", s, d) == 2) {
                svc_search(fd, s, d);           log_event(ip, port, "SEARCH %s->%s", s, d);
            } else {
                send_line(fd, "ERR Usage: SEARCH <source> <destination>");
                send_line(fd, "END");
                log_event(ip, port, "SEARCH - malformed");
            }
        }
        else if (!strcasecmp(cmd, "AVAIL"))   svc_avail (fd, arg, ip, port);
        else if (!strcasecmp(cmd, "BOOK"))    svc_book  (fd, arg, ip, port);
        else if (!strcasecmp(cmd, "CANCEL"))  svc_cancel(fd, arg, ip, port);
        else if (!strcasecmp(cmd, "STATUS"))  svc_status(fd, arg, ip, port);
        else if (!strcasecmp(cmd, "QUIT")) {
            send_line(fd, "OK Bye. Thank you for using Railway Reservation System.");
            log_event(ip, port, "QUIT - session ended by client");
            break;
        }
        else {
            send_line(fd, "ERR Invalid request '%s'", cmd);
            log_event(ip, port, "INVALID request '%s'", line);
        }
    }
    close(fd);                                  /* graceful close */
    printf("[*] Session finished with %s:%d\n\n", ip, port);
    log_event(ip, port, "CONNECTION CLOSED");
}

/* ------------------------ main (ITERATIVE) ------------------------ */
int main(void)
{
    int listenfd, connfd, opt = 1;
    struct sockaddr_in serv, cli;
    socklen_t clen;

    signal(SIGPIPE, SIG_IGN);
    init_database();

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); exit(1); }
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);

    memset(&serv, 0, sizeof serv);
    serv.sin_family      = AF_INET;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);
    serv.sin_port        = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&serv, sizeof serv) < 0) { perror("bind");  exit(1); }
    if (listen(listenfd, 5) < 0)                                   { perror("listen");exit(1); }

    printf("=== ITERATIVE Railway Reservation Server : port %d ===\n", PORT);
    printf("    (serves ONE client completely, then the next)\n\n");
    log_event("SERVER", 0, "Server started on port %d", PORT);

    /* ---- iterative loop : no fork / no threads ---- */
    while (1) {
        clen   = sizeof cli;
        connfd = accept(listenfd, (struct sockaddr *)&cli, &clen);
        if (connfd < 0) { if (errno == EINTR) continue; perror("accept"); continue; }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &cli.sin_addr, ip, sizeof ip);
        int port = ntohs(cli.sin_port);
        printf("[+] Client connected : %s : %d\n", ip, port);

        serve_client(connfd, ip, port);   /* blocking: others wait in backlog queue */
    }
    close(listenfd);
    return 0;
}
