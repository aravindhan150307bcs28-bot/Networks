#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT   9090
#define BUFSZ  4096
#define DLDIR  "client_downloads"

/* ---------- helpers (same protocol as server) ---------- */
static int send_all(int fd, const void *b, size_t n)
{ const char*p=b; while(n){ ssize_t s=send(fd,p,n,0); if(s<=0) return -1; p+=s; n-=s;} return 0; }

static int recv_all(int fd, void *b, size_t n)
{ char*p=b; while(n){ ssize_t r=recv(fd,p,n,0); if(r<=0) return -1; p+=r; n-=r;} return 0; }

static int recv_line(int fd, char *buf, size_t max)
{
    size_t i=0; char c;
    while (i < max-1) {
        ssize_t r = recv(fd,&c,1,0);
        if (r==0) return 0; if (r<0) return -1;
        if (c=='\n') break;
        buf[i++]=c;
    }
    buf[i]='\0';
    if (i && buf[i-1]=='\r') buf[i-1]='\0';
    return 1;
}

static int send_line(int fd, const char *fmt, ...)
{
    char l[BUFSZ]; va_list ap; va_start(ap,fmt);
    int n = vsnprintf(l,sizeof l-2,fmt,ap); va_end(ap);
    l[n++]='\n'; l[n]='\0';
    return send_all(fd,l,n);
}

static void trim(char *s){ s[strcspn(s,"\r\n")]='\0'; }

/* ---------------- upload ---------------- */
static void do_upload(int fd)
{
    char fname[256], path[300], reply[BUFSZ], buf[BUFSZ];

    printf("Enter path of file to upload : ");
    if (!fgets(path,sizeof path,stdin)) return;
    trim(path);

    FILE *fp = fopen(path,"rb");
    if (!fp) { printf(" !! Local file '%s' not found\n", path); return; }

    /* basename */
    char *b = strrchr(path,'/');
    strncpy(fname, b? b+1 : path, sizeof fname-1);
    fname[sizeof fname -1]='\0';

    fseek(fp,0,SEEK_END); long size=ftell(fp); rewind(fp);

    send_line(fd,"UPLOAD %s",fname);
    if (recv_line(fd,reply,sizeof reply)<=0) { fclose(fp); return; }
    if (strncmp(reply,"READY",5)!=0) { printf("Server: %s\n",reply); fclose(fp); return; }

    send_line(fd,"SIZE %ld",size);
    size_t n;
    while ((n=fread(buf,1,BUFSZ,fp))>0)
        if (send_all(fd,buf,n)<0) break;
    fclose(fp);

    if (recv_line(fd,reply,sizeof reply)>0) printf("Server: %s\n",reply);
}

/* ---------------- download ---------------- */
static void do_download(int fd)
{
    char fname[256], reply[BUFSZ], out[512], buf[BUFSZ];
    long size=0;

    printf("Enter file name to download : ");
    if (!fgets(fname,sizeof fname,stdin)) return;
    trim(fname);
    if (!*fname) { printf(" !! Empty file name\n"); return; }

    send_line(fd,"DOWNLOAD %s",fname);
    if (recv_line(fd,reply,sizeof reply)<=0) return;

    if (sscanf(reply,"SIZE %ld",&size)!=1) { printf("Server: %s\n",reply); return; }

    mkdir(DLDIR,0755);
    snprintf(out,sizeof out,"%s/%s",DLDIR,fname);
    FILE *fp=fopen(out,"wb");
    if (!fp){ printf(" !! Cannot create '%s'\n",out); return; }

    long left=size;
    while (left>0) {
        size_t chunk = (left>BUFSZ)?BUFSZ:(size_t)left;
        if (recv_all(fd,buf,chunk)<0){ printf(" !! Transfer interrupted\n"); break; }
        fwrite(buf,1,chunk,fp); left-=chunk;
    }
    fclose(fp);
    if (left==0) printf("Download complete : %s (%ld bytes)\n",out,size);
}

/* ---------------- main ---------------- */
int main(int argc,char *argv[])
{
    int fd; struct sockaddr_in serv;
    char line[BUFSZ], user[64], pass[64], choice[16];
    const char *ip = (argc>1)? argv[1] : "127.0.0.1";

    if ((fd=socket(AF_INET,SOCK_STREAM,0))<0){ perror("socket"); exit(1); }

    memset(&serv,0,sizeof serv);
    serv.sin_family=AF_INET;
    serv.sin_port=htons(PORT);
    if (inet_pton(AF_INET,ip,&serv.sin_addr)<=0){ perror("address"); exit(1); }

    if (connect(fd,(struct sockaddr*)&serv,sizeof serv)<0){ perror("connect"); exit(1); }
    printf("Connected to server %s:%d\n\n",ip,PORT);

    /* ---------- login ---------- */
    int ok=0;
    for (int a=0;a<3 && !ok;a++) {
        printf("Username : "); if(!fgets(user,sizeof user,stdin)) goto out; trim(user);
        printf("Password : "); if(!fgets(pass,sizeof pass,stdin)) goto out; trim(pass);

        send_line(fd,"LOGIN %s %s",user,pass);
        if (recv_line(fd,line,sizeof line)<=0) goto out;
        printf("Server: %s\n\n",line);
        if (!strncmp(line,"OK",2)) ok=1;
    }
    if (!ok) { printf("Authentication failed. Exiting.\n"); goto out; }

    /* ---------- menu ---------- */
    while (1) {
        printf("\n---------- MENU ----------\n");
        printf(" 1. Upload a file\n");
        printf(" 2. Download a file\n");
        printf(" 3. Server date & time\n");
        printf(" 4. Server system information\n");
        printf(" 5. Exit\n");
        printf("Enter choice : ");
        if (!fgets(choice,sizeof choice,stdin)) break;
        trim(choice);

        if (!strcmp(choice,"1"))       do_upload(fd);
        else if (!strcmp(choice,"2"))  do_download(fd);
        else if (!strcmp(choice,"3")) {
            send_line(fd,"TIME");
            if (recv_line(fd,line,sizeof line)<=0) break;
            printf("Server: %s\n",line);
        }
        else if (!strcmp(choice,"4")) {
            send_line(fd,"SYSINFO");
            if (recv_line(fd,line,sizeof line)<=0) break;
            printf("Server: %s\n",line);
        }
        else if (!strcmp(choice,"5")) {
            send_line(fd,"QUIT");
            if (recv_line(fd,line,sizeof line)>0) printf("Server: %s\n",line);
            printf("Session terminated.\n");
            break;
        }
        else {                                   /* invalid menu entry */
            printf(" !! Invalid choice, please select 1-5\n");
        }
    }
out:
    close(fd);
    return 0;
}
