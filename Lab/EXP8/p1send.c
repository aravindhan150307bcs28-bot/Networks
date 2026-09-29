#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define PORT 8085
#define BUFFER_SIZE 1024
#define TIMEOUT_SEC 3
#define QUEUE_SIZE 10

struct Frame {
    int seq_no;
    int ack_no;
    int command;
    char data[BUFFER_SIZE];
};

// Circular Queue structure for buffering messages
struct CircularQueue {
    char items[QUEUE_SIZE][BUFFER_SIZE];
    int front;
    int rear;
    int count;
};

void initQueue(struct CircularQueue *q) {
    q->front = 0;
    q->rear = -1;
    q->count = 0;
}

int isFull(struct CircularQueue *q) {
    return q->count == QUEUE_SIZE;
}

int isEmpty(struct CircularQueue *q) {
    return q->count == 0;
}

void enqueue(struct CircularQueue *q, char *data) {
    if (isFull(q)) {
        printf("[Queue] Warning: Message queue is full!\n");
        return;
    }
    q->rear = (q->rear + 1) % QUEUE_SIZE;
    strcpy(q->items[q->rear], data);
    q->count++;
}

void dequeue(struct CircularQueue *q, char *data) {
    if (isEmpty(q)) return;
    strcpy(data, q->items[q->front]);
    q->front = (q->front + 1) % QUEUE_SIZE;
    q->count--;
}

int main() {
    int sockfd;
    struct sockaddr_in servaddr;
    struct Frame frame, ack_frame;
    socklen_t len;
    int send_seq = 0;
    char input[BUFFER_SIZE];
    int choice;
    struct CircularQueue messageQueue;

    initQueue(&messageQueue);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("\n===========================================\n");
    printf("       Stop-and-Wait ARQ Test Menu         \n");
    printf("===========================================\n");
    printf("1. Normal Transmission (Happy Path / Seq Toggle)\n");
    printf("2. Test Timeout & Retransmission (Drop Frame)\n");
    printf("3. Test Duplicate Frame Handling (Manual Resend)\n");
    printf("Select test case option: ");

    if (scanf("%d", &choice) != 1) {
        close(sockfd);
        exit(0);
    }
    getchar(); // Clear trailing newline

    while (1) {
        printf("\nEnter data message (or type 'exit' to quit): ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;

        if (strcmp(input, "exit") == 0) break;

        // Enqueue the message into the circular queue
        enqueue(&messageQueue, input);

        // Process message from circular queue using Stop-and-Wait ARQ
        while (!isEmpty(&messageQueue)) {
            char current_msg[BUFFER_SIZE];
            dequeue(&messageQueue, current_msg);

            strcpy(frame.data, current_msg);
            frame.seq_no = send_seq;
            frame.command = (choice == 2) ? 2 : 0;

            int acknowledged = 0;
            while (!acknowledged) {
                len = sizeof(servaddr);
                printf("[Timer Started] Sending Frame %d (Seq: %d)...\n", send_seq, frame.seq_no);
                sendto(sockfd, &frame, sizeof(frame), 0, (const struct sockaddr *)&servaddr, len);

                len = sizeof(servaddr);
                int n = recvfrom(sockfd, &ack_frame, sizeof(ack_frame), 0, (struct sockaddr *)&servaddr, &len);

                if (n < 0) {
                    printf("[Timeout Expired!] No ACK received. Resending Frame %d...\n", send_seq);
                    frame.command = 0; // Clear drop flag for subsequent retransmissions
                } else {
                    if (ack_frame.ack_no == (send_seq + 1) % 2) {
                        printf("[Timer Stopped] Valid ACK %d received.\n", ack_frame.ack_no);
                        send_seq = (send_seq + 1) % 2; // Toggle sequence number
                        acknowledged = 1;
                    } else {
                        printf("[Error] Unexpected ACK received. Retransmitting...\n");
                    }
                }
            }

            // Option 3: Clean manual duplicate injection prompt
            if (choice == 3) {
                char duplicate_choice;
                printf("Do you want to manually re-send this frame as a duplicate to test receiver detection? (y/n): ");
                scanf(" %c", &duplicate_choice);
                getchar(); // Clear newline

                if (duplicate_choice == 'y' || duplicate_choice == 'Y') {
                    int old_seq = (send_seq + 1) % 2;
                    printf("[Action] Manually re-sending Frame with old Seq: %d...\n", old_seq);
                    frame.seq_no = old_seq;
                    frame.command = 0;

                    len = sizeof(servaddr);
                    sendto(sockfd, &frame, sizeof(frame), 0, (const struct sockaddr *)&servaddr, len);

                    len = sizeof(servaddr);
                    recvfrom(sockfd, &ack_frame, sizeof(ack_frame), 0, (struct sockaddr *)&servaddr, &len);
                    printf("[Info] Received ACK %d back.\n", ack_frame.ack_no);

                    frame.seq_no = send_seq;
                }
            }
        }
    }

    close(sockfd);
    return 0;
}
