#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define MESSAGE_QUEUE_KEY 314159
#define MAX_SIZE 128

typedef struct {
    long message_type;
    char message_content[MAX_SIZE];
} MessageBuffer;

int main() {
    int message_queue_id;
    key_t message_queue_key = MESSAGE_QUEUE_KEY;
    MessageBuffer receive_buffer;
    size_t buf_length = sizeof(receive_buffer) - sizeof(long);

    if ((message_queue_id = msgget(message_queue_key, IPC_CREAT | 0666)) < 0) {
        perror("Failed to access message queue in print server");
        return -1;
    }

    printf("Print server started. Listening for messages...\n");

    while (1) {
        if (msgrcv(message_queue_id, &receive_buffer, buf_length, 1, 0) < 0) {
            perror("Failed to receive message in print server");
            return -1;
        }
        printf("%s\n", receive_buffer.message_content);
    }

    return 0;
}
