#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8085
#define BUFFER_SIZE 1024
#define MAX_SEQ 16
#define WINDOW_SIZE 8

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
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    struct Frame recv_frame;
    struct Ack ack_frame;
    int recv_window[MAX_SEQ] = {0};
    char circular_buffer[MAX_SEQ][BUFFER_SIZE];
    int Rn = 0;

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Selective Repeat Duplicate-Test Receiver active on port %d...\n", PORT);

    while (1) {
        memset(&recv_frame, 0, sizeof(recv_frame));
        len = sizeof(cliaddr);
        int n = recvfrom(sockfd, &recv_frame, sizeof(recv_frame), 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) continue;

        if (recv_frame.command == 2) {
            printf("\n[Test Simulation] Intentionally dropping Frame %d!\n", recv_frame.seq_no);
            continue;
        }

        int seq = recv_frame.seq_no;
        printf("\n[Received] Frame Seq: %d | Data: %s\n", seq, recv_frame.data);

        int in_window = 0;
        for (int i = 0; i < WINDOW_SIZE; i++) {
            if ((Rn + i) % MAX_SEQ == seq) {
                in_window = 1;
                break;
            }
        }

        memset(&ack_frame, 0, sizeof(ack_frame));
        if (in_window) {
            if (recv_window[seq]) {
                printf("[Action] Duplicate frame %d detected. Discarding duplicate data and re-sending ACK.\n", seq);
            } else {
                recv_window[seq] = 1;
                strcpy(circular_buffer[seq], recv_frame.data);
                printf("[Action] Frame %d buffered in circular slot.\n", seq);
            }

            ack_frame.ack_no = seq;
            sendto(sockfd, &ack_frame, sizeof(ack_frame), 0, (struct sockaddr *)&cliaddr, len);
            printf("[Sent] ACK for Frame %d\n", seq);

            while (recv_window[Rn]) {
                printf("[Window Slide] Delivering Frame %d: %s\n", Rn, circular_buffer[Rn]);
                recv_window[Rn] = 0;
                Rn = (Rn + 1) % MAX_SEQ;
            }
        } else {
            // Also handle older already-delivered frames falling behind Rn as duplicates
            ack_frame.ack_no = seq;
            sendto(sockfd, &ack_frame, sizeof(ack_frame), 0, (struct sockaddr *)&cliaddr, len);
            printf("[Action] Out-of-range or old frame %d. Re-sending ACK.\n", seq);
        }
    }
    close(sockfd);
    return 0;
}
