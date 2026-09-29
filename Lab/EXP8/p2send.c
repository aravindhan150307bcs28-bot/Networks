#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>

#define PORT 8085
#define BUFFER_SIZE 1024
#define MAX_SEQ 16
#define WINDOW_SIZE 3
#define TIMEOUT_SEC 3

struct Frame {
    int seq_no;
    int command;
    char data[BUFFER_SIZE];
};

struct Ack {
    int ack_no;
};

int main() {
    int sockfd;
    struct sockaddr_in servaddr, from_addr;
    socklen_t len, from_len;

    int Sf = 0;
    int Sn = 0;
    int choice = 0;

    struct Frame window_frames[MAX_SEQ];
    int window_acks[MAX_SEQ];
    time_t timer_start[MAX_SEQ];

    for(int i = 0; i < MAX_SEQ; i++) {
        memset(&window_frames[i], 0, sizeof(struct Frame));
        window_acks[i] = 0;
        timer_start[i] = 0;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("\n===========================================\n");
    printf("     Selective Repeat ARQ Test Menu        \n");
    printf("===========================================\n");
    printf("1. Normal Transmission\n");
    printf("2. Test Timeout & Retransmission (Drop Frame 0)\n");
    printf("3. Test Duplicate Frame Handling (Manual Resend)\n");
    printf("Select test case option: ");
    if (scanf("%d", &choice) != 1) choice = 1;
    getchar(); // Clear trailing newline

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    printf("Selective Repeat Sender Active. Type messages:\n> ");
    fflush(stdout);

    while (1) {
        if (((Sn - Sf + MAX_SEQ) % MAX_SEQ) < WINDOW_SIZE) {
            char input[BUFFER_SIZE];
            if (fgets(input, sizeof(input), stdin) != NULL) {
                input[strcspn(input, "\n")] = 0;
                if (strlen(input) > 0) {
                    int current_sent_seq = Sn;
                    window_frames[current_sent_seq].seq_no = current_sent_seq;
                    window_frames[current_sent_seq].command = (choice == 2 && current_sent_seq == 0) ? 2 : 0;
                    strcpy(window_frames[current_sent_seq].data, input);
                    window_acks[current_sent_seq] = 0;
                    timer_start[current_sent_seq] = time(NULL);

                    len = sizeof(servaddr);
                    sendto(sockfd, &window_frames[current_sent_seq], sizeof(window_frames[current_sent_seq]), 0, (const struct sockaddr *)&servaddr, len);
                    printf("[Sent] Frame %d: %s\n> ", current_sent_seq, input);
                    fflush(stdout);

                    Sn = (Sn + 1) % MAX_SEQ;

                    // Option 3: Prompt to inject a duplicate transmission test
                    if (choice == 3) {
                        char dup_choice;
                        printf("Manually re-send Frame %d as duplicate? (y/n): ", current_sent_seq);
                        fflush(stdout);

                        // Temporarily block to capture user decision for duplicate test
                        int flags_stdin = fcntl(STDIN_FILENO, F_GETFL, 0);
                        fcntl(STDIN_FILENO, F_SETFL, flags_stdin & ~O_NONBLOCK);

                        if (scanf(" %c", &dup_choice) == 1 && (dup_choice == 'y' || dup_choice == 'Y')) {
                            printf("[Action] Forcing duplicate transmission of Frame %d...\n> ", current_sent_seq);
                            sendto(sockfd, &window_frames[current_sent_seq], sizeof(window_frames[current_sent_seq]), 0, (const struct sockaddr *)&servaddr, len);
                        }
                        getchar(); // Clear newline
                        fcntl(STDIN_FILENO, F_SETFL, flags_stdin); // Restore non-blocking
                    }
                }
            }
        }

        struct Ack ack_frame;
        memset(&ack_frame, 0, sizeof(ack_frame));
        from_len = sizeof(from_addr);
        int n = recvfrom(sockfd, &ack_frame, sizeof(ack_frame), 0, (struct sockaddr *)&from_addr, &from_len);

        if (n == sizeof(struct Ack)) {
            int ack_seq = ack_frame.ack_no;
            if (ack_seq >= 0 && ack_seq < MAX_SEQ) {
                printf("\n[Received] ACK for Frame %d\n> ", ack_seq);
                fflush(stdout);

                window_acks[ack_seq] = 1;

                while (window_acks[Sf]) {
                    window_acks[Sf] = 0;
                    Sf = (Sf + 1) % MAX_SEQ;
                    printf("[Window Slide] Circular Base advanced to %d.\n> ", Sf);
                    fflush(stdout);
                }
            }
        }

        time_t current_time = time(NULL);
        int count = (Sn - Sf + MAX_SEQ) % MAX_SEQ;
        for (int i = 0; i < count; i++) {
            int current_seq = (Sf + i) % MAX_SEQ;
            if (!window_acks[current_seq]) {
                if (current_time - timer_start[current_seq] >= TIMEOUT_SEC) {
                    printf("\n[Timeout Expired!] Retransmitting Frame %d alone.\n> ", current_seq);
                    fflush(stdout);

                    window_frames[current_seq].command = 0;
                    len = sizeof(servaddr);
                    sendto(sockfd, &window_frames[current_seq], sizeof(window_frames[current_seq]), 0, (const struct sockaddr *)&servaddr, len);
                    timer_start[current_seq] = time(NULL);
                }
            }
        }
        usleep(100000);
    }
    close(sockfd);
    return 0;
}
