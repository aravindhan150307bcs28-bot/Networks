
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT      9001
#define SERVER_IP "127.0.0.1"
#define BUFSZ     4096
#define DOWNDIR   "client_files"

static int send_all(int s, const void *b, size_t n)
{ const char *p=b; while(n){ssize_t k=send(s,p,n,0); if(k<=0)return -1; p+=k;n-=k;} return 0; }
static int recv_all(int s, void *b, size_t n)
{ char *p=b; while(n){ssize_t k=recv(s,p,n,0); if(k<=0)return -1; p+=k;n-=k;} return 0; }
static int recv_line(int s, char *b, int max)
{ int i=0; char c; while(i<max-1){ssize_t k=recv(s,&c,1,0); if(k<=0)return -1;
  if(c=='\n')break; if(c!='\r')b[i++]=c;} b[i]='\0'; return i; }
static int send_line(int s, const char *fmt, ...)
{ char t[BUFSZ]; va_list ap; va_start(ap,fmt); vsnprintf(t,sizeof t-2,fmt,ap); va_end(ap);
  strcat(t,"\n"); return send_all(s,t,strlen(t)); }

static void chomp(char *s){ s[strcspn(s,"\r\n")] = '\0'; }
static const char *base_name(const char *p){ const char *s=strrchr(p,'/'); return s?s+1:p; }

int main(void)
{
    int s; struct sockaddr_in serv; char line[BUFSZ];

    mkdir(DOWNDIR, 0777);

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) { perror("socket"); exit(1); }
    memset(&serv, 0, sizeof serv);
    serv.sin_family = AF_INET;
    serv.sin_port   = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);

    if (connect(s, (struct sockaddr*)&serv, sizeof serv) < 0) { perror("connect"); exit(1); }

    recv_line(s, line, sizeof line);
    printf("SERVER: %s\n", line);

    /* ---------- login ---------- */
    int logged = 0;
    while (!logged) {
        char u[64], p[64];
        printf("\nUsername: "); if(!fgets(u,sizeof u,stdin)) return 0; chomp(u);
        printf("Password: ");   if(!fgets(p,sizeof p,stdin)) return 0; chomp(p);
        send_line(s, "LOGIN %s %s", u, p);
        if (recv_line(s, line, sizeof line) < 0) { printf("Server closed.\n"); return 0; }
        printf("SERVER: %s\n", line);
        if (!strncmp(line, "OK", 2)) logged = 1;
        else if (strstr(line, "Too many")) return 0;
    }

    /* ---------- menu ---------- */
    for (;;) {
        printf("\n=========== MENU ===========\n"
               "1. Upload a file\n"
               "2. Download a file\n"
               "3. Server date & time\n"
               "4. Server system information\n"
               "5. Exit\n"
               "Choice: ");
        char ch[16];
        if (!fgets(ch, sizeof ch, stdin)) break;
        int choice = atoi(ch);

        if (choice == 1) {                                  /* UPLOAD */
            char f[256];
            printf("Local file to upload: ");
            if(!fgets(f,sizeof f,stdin)) break; chomp(f);
            FILE *fp = fopen(f, "rb");
            if (!fp) { printf("!! Local file not found\n"); continue; }
            struct stat st; stat(f, &st);
            send_line(s, "UPLOAD %s %ld", base_name(f), (long)st.st_size);
            recv_line(s, line, sizeof line);
            if (strncmp(line, "READY", 5)) { printf("SERVER: %s\n", line); fclose(fp); continue; }
            char buf[BUFSZ]; size_t n;
            while ((n = fread(buf,1,BUFSZ,fp)) > 0) send_all(s, buf, n);
            fclose(fp);
            recv_line(s, line, sizeof line);
            printf("SERVER: %s\n", line);
        }
        else if (choice == 2) {                             /* DOWNLOAD */
            char f[256];
            printf("Remote file to download: ");
            if(!fgets(f,sizeof f,stdin)) break; chomp(f);
            send_line(s, "DOWNLOAD %s", f);
            if (recv_line(s, line, sizeof line) < 0) break;
            if (strncmp(line, "OK", 2)) { printf("SERVER: %s\n", line); continue; }
            long size = atol(line + 3);
            char path[512]; snprintf(path,sizeof path,"%s/%s",DOWNDIR,base_name(f));
            FILE *fp = fopen(path, "wb");
            if (!fp) { printf("!! cannot write locally\n"); continue; }
            char buf[BUFSZ]; long left = size;
            while (left > 0) {
                size_t c = (left > BUFSZ) ? BUFSZ : (size_t)left;
                if (recv_all(s, buf, c) < 0) break;
                fwrite(buf,1,c,fp); left -= c;
            }
            fclose(fp);
            printf("Downloaded %ld bytes -> %s\n", size - left, path);
        }
        else if (choice == 3) {                             /* TIME */
            send_line(s, "TIME");
            recv_line(s, line, sizeof line);
            printf("SERVER: %s\n", line);
        }
        else if (choice == 4) {                             /* SYSINFO */
            send_line(s, "SYSINFO");
            while (recv_line(s, line, sizeof line) > 0 && strcmp(line, "END"))
                printf("%s\n", line);
        }
        else if (choice == 5) {                             /* QUIT */
            send_line(s, "QUIT");
            recv_line(s, line, sizeof line);
            printf("SERVER: %s\n", line);
            break;
        }
        else printf("!! Invalid choice, try again.\n");
    }
    close(s);
    printf("Disconnected.\n");
    return 0;
}
