#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8085
#define BUFFER_SIZE 1024

struct Frame {
    int seq_no;
    int ack_no;
    int command; // 0: Normal, 2: Simulate Drop
    char data[BUFFER_SIZE];
};

int main() {
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    struct Frame recv_frame, send_frame;
    int expected_seq = 0;

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

    printf("Unified ARQ Receiver active on port %d...\n", PORT);

    while (1) {
        len = sizeof(cliaddr);
        int n = recvfrom(sockfd, &recv_frame, sizeof(recv_frame), 0, (struct sockaddr *)&cliaddr, &len);
        if (n < 0) continue;

        // Check command header for test simulations
        if (recv_frame.command == 2) {
            printf("\n[Test Simulation] Intentionally dropping Frame %d to test Timeout & Retransmission!\n", recv_frame.seq_no);
            continue; // Skip sending ACK, forcing sender timeout
        }

        printf("\n[Received] Frame Seq No: %d | Data: %s\n", recv_frame.seq_no, recv_frame.data);

        if (recv_frame.seq_no == expected_seq) {
            printf("[Action] Valid frame. Delivering data.\n");
            expected_seq = (expected_seq + 1) % 2;
        } else {
            printf("[Action] Duplicate frame detected (Expected %d, got %d). Discarding data.\n", expected_seq, recv_frame.seq_no);
        }

        send_frame.ack_no = expected_seq;
        sendto(sockfd, &send_frame, sizeof(send_frame), 0, (struct sockaddr *)&cliaddr, len);
        printf("[Sent] ACK %d\n", send_frame.ack_no);
    }

    close(sockfd);
    return 0;
}
