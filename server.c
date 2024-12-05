#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define SERVER_FIFO "serverFIFO"

typedef struct {
    char sender[50];
    char recipient[50];
    char content[200];
} Message;

void terminate(int signal) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server_fd, placeholder_fd;
    Message msg;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    server_fd = open(SERVER_FIFO, O_RDONLY);
    if (server_fd == -1) {
        perror("Unable to open server FIFO");
        exit(1);
    }

    placeholder_fd = open(SERVER_FIFO, O_WRONLY);
    if (placeholder_fd == -1) {
        perror("Unable to open placeholder FD");
        close(server_fd);
        exit(1);
    }

    while (1) {
        ssize_t bytes_read = read(server_fd, &msg, sizeof(Message));
        if (bytes_read == sizeof(Message)) {
            if (strlen(msg.sender) == 0 || strlen(msg.recipient) == 0 || strlen(msg.content) == 0) {
                continue;
            }

            printf("Received a request from %s to send the message %s to %s.\n",
                   msg.sender, msg.content, msg.recipient);

            int recipient_fd = open(msg.recipient, O_WRONLY);
            if (recipient_fd == -1) {
                printf("Can't open target FIFO for %s.\n", msg.recipient);
                continue;
            }

            ssize_t bytes_written = write(recipient_fd, &msg, sizeof(Message));
            if (bytes_written == -1) {
                perror("Failed to write to recipient FIFO");
            }

            close(recipient_fd);
        } else if (bytes_read == -1) {
            perror("Error reading from server FIFO");
        }
    }

    close(server_fd);
    close(placeholder_fd);
    return 0;
}
