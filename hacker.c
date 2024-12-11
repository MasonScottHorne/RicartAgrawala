#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MAX_SIZE 128
#define MESSAGE_QUEUE_KEY 314159

typedef struct {
    long message_type;
    char message_content[MAX_SIZE];
} MessageBuffer;

void print(const char *message) {
    if (!message) {
        fprintf(stderr, "Error: message is NULL\n");
        return;
    }

    int message_queue_id = msgget(MESSAGE_QUEUE_KEY, IPC_CREAT | 0666);
    if (message_queue_id < 0) {
        perror("Failed to access message queue in print");
        return;
    }

    MessageBuffer send_buffer = { .message_type = 1 };
    snprintf(send_buffer.message_content, sizeof(send_buffer.message_content), "%s", message);

    if (msgsnd(message_queue_id, &send_buffer, sizeof(send_buffer) - sizeof(long), IPC_NOWAIT) < 0) {
        perror("Failed to send message in print");
        return;
    }

    printf("Message sent: %s\n", message);
}

int main() {
    print("HACKER Ja ja ja");
    print("hello world");
    return 0;
}
