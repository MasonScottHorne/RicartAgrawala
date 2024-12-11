#include <stdio.h>   
#include <stdlib.h>   
#include <string.h>   
#include <unistd.h>      
#include <sys/ipc.h>    
#include <sys/msg.h>     
#include <sys/shm.h>     
#include <sys/stat.h>    
#include <time.h>        
#include <sys/types.h>   
#include <sys/sem.h>    

#define MAX_SIZE 128                       // Max size for message content
#define MESSAGE_QUEUE_KEY 314159           // Key for the print message queue
#define COMMUNICATION_QUEUE_KEY 27182      // Key for the communication message queue
#define MAX_NODES 10                       // Max number of nodes supported
#define NODES_NEEDED_TO_PARTICIPATE 4      // Min number of nodes required to participate
#define CRITICAL_SECTION_ITERATIONS 4       // Number of iterations inside the critical section

// Shared variables among nodes
typedef struct SharedVariables {
    int N;                                 // Total number of participating nodes
    int request_number;                    // Sequence number of the current request
    int highest_request_number;            // Highest request number observed so far
    int outstanding_reply;                 // Number of pending replies needed
    int request_cs;                        // (1 = requesting, 0 = not) Flag indicating if the node is requesting critical section 
    int reply_deferred[MAX_NODES];         // (1 = deferred, 0 = not) Array to track deferred replies to other nodes 
    int mutex_sem;                         // Semaphore ID for mutual exclusion
    int wait_sem;                          // Semaphore ID for waiting on replies
} SharedVariables;

// Message types
typedef enum { 
    REQUEST,    // Node Requesting access to the critical section
    REPLY,      // Node Reply to a REQUEST message
} msg_type;

// Communication queue struct
typedef struct {
    long message_type;             // Receiver node ID (each node listens to its own ID)
    msg_type type;                 // Type of the message 
    int request_sequence_number;   // Sequence number of the request
    int senderID;                  // ID of the sender node
} Message;

// Print server struct
typedef struct {
    long message_type;             // Message type (print server)
    char message_content[MAX_SIZE]; // Content of the message to be printed
} MessageBuffer;

// Perform semaphore operations (P/V)
void semaphore_op(int semid, int semnum, int op) {
    struct sembuf operation = {semnum, op, 0}; // Sembuf structure
    if (semop(semid, &operation, 1) < 0) {    // Semaphore operation
        perror("Semaphore operation failed");  
        exit(EXIT_FAILURE);                   
    }
}

// Wait (P operation)
void semaphore_wait(int semid, int semnum) {
    semaphore_op(semid, semnum, -1); // Decrement semaphore value
}

// Signal (V operation)
void semaphore_signal(int semid, int semnum) {
    semaphore_op(semid, semnum, 1);  // Increment semaphore value
}

// Sending a message to the print server that indicates entry into the critical section
void print_to_server(const char *message) {
    int message_queue_id;                           // ID of the message queue
    int message_flag = IPC_CREAT | 0666;           // Flags to create the queue with read/write permissions
    key_t message_queue_key = MESSAGE_QUEUE_KEY;    // Key for the print message queue
    MessageBuffer send_buffer = {.message_type = 1}; // Message buffer message_type = 1
    size_t buf_length = sizeof(send_buffer) - sizeof(long); // Calculation for the size of the message excluding the type

    // Get or create the message queue for printing
    if ((message_queue_id = msgget(message_queue_key, message_flag)) < 0) {
        perror("msgget error"); 
        exit(EXIT_FAILURE);      
    }

    // Copy the message content into the buffer, ensuring no overflow
    strncpy(send_buffer.message_content, message, sizeof(send_buffer.message_content) - 1);
    send_buffer.message_content[sizeof(send_buffer.message_content) - 1] = '\0'; // Ensure null-termination

    // Send the message to the print server without blocking
    if (msgsnd(message_queue_id, &send_buffer, buf_length, IPC_NOWAIT) < 0) {
        perror("msgsnd error"); 
        exit(EXIT_FAILURE);      
    } else {
        // Message has been sent to the print server
        printf("This node is currently sending a message to the print server to be in the Critical Section.\n");
    }
}

// Receives a message from the communication message queue for this node 
Message receive_message(long message_type) {
    int message_queue_id;                           // ID of the message queue
    int message_flag = IPC_CREAT | 0666;           // Flags to create the queue with read/write permissions
    key_t communication_queue_key = COMMUNICATION_QUEUE_KEY; // Key for the communication message queue

    // Get or create the communication message queue
    if ((message_queue_id = msgget(communication_queue_key, message_flag)) < 0) {
        perror("msgget error");
        exit(EXIT_FAILURE);   
    }

    Message receive_buffer; // Buffer to store the received message

    // Receive the message intended for this node from the communication queue
    if (msgrcv(message_queue_id, &receive_buffer, sizeof(Message) - sizeof(long), message_type, 0) < 0) {
        perror("msgrcv error"); 
        exit(EXIT_FAILURE);     
    }
    return receive_buffer; // Return the received message
}

// Function to send a message to another node
int send_message(long receiverID, msg_type type, int request_sequence_number, int senderID) {
    if (receiverID <= 0) {
        fprintf(stderr, "Error: Invalid receiver ID %ld\n", receiverID); // Print error message
        return -1; // Return an error code if the receiver ID is invalid
    }

    int message_queue_id;                           // ID of the communication message queue
    int message_flag = IPC_CREAT | 0666;           // Flags to create the queue with read/write permissions
    key_t communication_queue_key = COMMUNICATION_QUEUE_KEY; // Key for the communication message queue

    // Get or create the communication message queue
    if ((message_queue_id = msgget(communication_queue_key, message_flag)) < 0) {
        perror("msgget error"); // Print error message
        exit(EXIT_FAILURE);      // Exit the program with a failure status
    }

    Message send_buffer = {0};                     // Message buffer to store the message to be sent
    send_buffer.message_type = receiverID;          // Set the receiver's node ID as the message type
    send_buffer.type = type;                        // Set the message type 
    send_buffer.request_sequence_number = request_sequence_number; // Set the request sequence number
    send_buffer.senderID = senderID;                // Set the sender's node ID

    size_t buf_length = sizeof(send_buffer) - sizeof(long); // Calculate the size of the message excluding the type

    // Send the message to the communication queue
    if (msgsnd(message_queue_id, &send_buffer, buf_length, IPC_NOWAIT) < 0) {
        perror("msgsnd error"); 
        exit(EXIT_FAILURE);   
    }

    // Confirmation
    printf("Message sent to %ld, type is %d, request number is %d, sender ID is %d\n",
           send_buffer.message_type, send_buffer.type, send_buffer.request_sequence_number, send_buffer.senderID);

    return 0; 
}

int main(int argc, char *argv[]) {
    int me; // Store this node's ID

    // Ensure that exactly one argument: node ID is provided
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <node_id>\n", argv[0]); 
        exit(1); 
    }

    me = atoi(argv[1]); // Convert the first argument to an integer node ID

    // Validate positive integer node ID
    if (me <= 0) {
        fprintf(stderr, "Node ID must be a positive integer.\n"); 
        exit(1); 
    }

    printf("Starting Node %d\n", me); 

    srand(time(NULL));

    SharedVariables *node_state; // Pointer to shared variables structure

    // Shared memory for shared variables
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedVariables), 0600); // Shared memory with read/write permissions
    if (shm_id < 0) {
        perror("shmget error"); 
        exit(EXIT_FAILURE);      
    }

    node_state = shmat(shm_id, NULL, 0); // Attach shared memory to process's address space
    if (node_state == (void *) -1) {
        perror("shmat error"); 
        exit(EXIT_FAILURE);      
    }

    // 2 semaphores for mutual exclusion and waiting
    int semid = semget(IPC_PRIVATE, 2, IPC_CREAT | 0600); // Shared memory with read/write permissions
    if (semid < 0) {
        perror("semget error"); 
        exit(EXIT_FAILURE);      
    }

    semctl(semid, 0, SETVAL, 1); // Initialize semaphore 0 mutex_sem to 1 for mutual exclusion
    semctl(semid, 1, SETVAL, 0); // Initialize semaphore 1 wait_sem to 0 for waiting on replies

    node_state->N = NODES_NEEDED_TO_PARTICIPATE;            
    node_state->request_number = 0;                         
    node_state->highest_request_number = 0;                 
    node_state->outstanding_reply = 0;                     
    node_state->request_cs = 0;                            
    node_state->mutex_sem = semid;                         
    node_state->wait_sem = semid;                           

    pid_t pid = fork(); // Create child process

    if (pid == 0) { // Child process: for handling incoming messages
        while (1) { // Infinite loop to continuously receive and process messages
            // Receive a message intended for this node
            Message ret = receive_message(me);

            // Print details about the received message
            printf("Received message sent to %ld, type: %d, request number: %d, from: %d\n",
                   ret.message_type, ret.type, ret.request_sequence_number, ret.senderID);

            if (ret.type == REQUEST) { // If the message is a REQUEST to enter the critical section
                int k = ret.request_sequence_number; // Extract the request sequence number
                int i = ret.senderID;                 // Extract the sender's node ID
                int defer_it = 0;                     // Flag to determine if the reply should be deferred

                // Update the highest request number if the received request is higher
                if (k > node_state->highest_request_number) {
                    node_state->highest_request_number = k;
                }

                semaphore_wait(node_state->mutex_sem, 0); // Enter critical section by waiting 

                // Determine whether to defer the reply based on the Ricart-Agrawala conditions
                defer_it = (node_state->request_cs) && // If this node is requesting the critical section
                           ((k > node_state->request_number) || // And the incoming request has a higher sequence number
                           ((k == node_state->request_number) && (i > me))); // Or the same sequence number but higher node ID

                semaphore_signal(node_state->mutex_sem, 0); // Exit critical section by signaling 

                if (defer_it) { // If the reply should be deferred
                    printf("Node %d deferring REPLY to %d\n", me, i);
                    node_state->reply_deferred[i] = 1; // Mark the reply to node i as deferred
                } else { // If the reply should be sent immediately
                    send_message(i, REPLY, -1, me); // Send a REPLY message to node i
                }
            } else if (ret.type == REPLY) { // If the message is a REPLY to a REQUEST
                printf("Node %d received REPLY from %d\n", me, ret.senderID);
                node_state->outstanding_reply -= 1; // Decrement the count of outstanding replies
                semaphore_signal(node_state->wait_sem, 1); // Signal the wait_sem to indicate a reply has been received
            }
        }
    } else { // Parent process: responsible for requesting and entering the critical section
        sleep(1); // Sleep to ensure child process is ready

        while (1) { // Infinite loop 
            int random_delay = rand() % 10 + 1; // 1-10 seconds random delay before starting
            sleep(random_delay); // Sleep for the random delay to randomize the start times of nodes

            printf("Node %d requesting critical section\n", me);

            semaphore_wait(node_state->mutex_sem, 0); // Enter critical section by waiting
            node_state->request_cs = 1;               // Indicate that this node is requesting the critical section
            node_state->request_number = node_state->highest_request_number + 1; // Increment the request sequence number
            semaphore_signal(node_state->mutex_sem, 0); // Exit critical section by signaling 

            node_state->outstanding_reply = node_state->N - 1; // Count of outstanding replies needed
            printf("Request Number is %d\n", node_state->request_number);
            printf("%d replies needed\n", node_state->outstanding_reply);

            // Send REQUEST messages to all other nodes
            for (int i = 1; i <= node_state->N; i++) {
                if (i != me) { // Do not send a REQUEST to itself
                    printf("Node %d sending REQUEST to %d\n", me, i);
                    send_message(i, REQUEST, node_state->request_number, me); // Send a REQUEST message to node i
                }
            }

            printf("Node %d waiting for replies\n", me);

            // Wait until all outstanding replies have been received
            while (node_state->outstanding_reply > 0) {
                semaphore_wait(node_state->wait_sem, 1); // Wait on wait_sem for a REPLY
            }

            // All necessary replies have been received; Enter the critical section
            printf("Node %d entering critical section\n", me);
            char buffer[MAX_SIZE]; // Buffer to hold messages to be printed

            // Send a start message to the print server
            sprintf(buffer, "########## START OUTPUT FOR NODE %d ##########", me);
            print_to_server(buffer);

            // Inside the critical section
            int cs_duration = rand() % 4 + 4; // 4-7 seconds random duration of the critical section

            // Print this is line xyz Inside the critical section
            for (int i = 0; i < CRITICAL_SECTION_ITERATIONS; i++) {
                sleep(cs_duration / CRITICAL_SECTION_ITERATIONS); // Sleep for a portion of the critical section duration
                sprintf(buffer, "Node %d printing: this is line %d", me, i + 1); // This is line xyz message
                print_to_server(buffer); // Send the message to the print server
            }

            // Send an end message to the print server
            sprintf(buffer, "---------- END OUTPUT FOR NODE %d ----------", me);
            print_to_server(buffer);

            printf("Node %d leaving critical section\n", me);

            semaphore_wait(node_state->mutex_sem, 0); // Enter critical section to update shared variables
            node_state->request_cs = 0;               // Indicate that this node is no longer requesting the critical section

            // Send deferred REPLY messages to nodes that had their replies deferred
            for (int i = 1; i <= node_state->N; i++) {
                if (node_state->reply_deferred[i]) { // Check if reply was deferred to node i
                    printf("Node %d sending deferred REPLY to %d\n", me, i);
                    send_message(i, REPLY, -1, me); // Send REPLY message to node i
                    node_state->reply_deferred[i] = 0; // Reset deferred reply flag for node i
                }
            }

            semaphore_signal(node_state->mutex_sem, 0); // Exit critical section by signaling
        }
        // int random_sleep = rand() % 10 + 1; // 1-10 seconds random delay before starting
        // sleep(random_sleep); // Sleep 
    }

    return 0;
}
