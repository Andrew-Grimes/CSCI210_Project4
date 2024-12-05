#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define SERVER_FIFO "serverFIFO"

struct message {
    char source[50];
    char target[50];
    char msg[200];
};

void terminate(int signal) {
    unlink(SERVER_FIFO);
    exit(0);
}

int main() {
    int server_fd, placeholder_fd;
    struct message msg;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    unlink(SERVER_FIFO);

    if (mkfifo(SERVER_FIFO, 0666) < 0) {
        perror("Can't create server FIFO");
        exit(EXIT_FAILURE);
    }

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
        ssize_t bytes_read = read(server_fd, &msg, sizeof(struct message));
        if (bytes_read == sizeof(struct message)) {
            if (strlen(msg.source) == 0 || strlen(msg.target) == 0 || strlen(msg.msg) == 0) {
                continue;
            }

            printf("Received a request from %s to send the message %s to %s.\n",
                   msg.source, msg.msg, msg.target);

            int recipient_fd = open(msg.target, O_WRONLY);
            if (recipient_fd == -1) {
                printf("Can't open target FIFO for %s.\n", msg.target);
                continue;
            }

            ssize_t bytes_written = write(recipient_fd, &msg, sizeof(struct message));
            if (bytes_written == -1) {
                perror("Failed to write to recipient FIFO");
            }

            close(recipient_fd);
        } else if (bytes_read == -1) {
            perror("Error reading from server FIFO");
        } else if (bytes_read == 0) {
            close(server_fd);
            server_fd = open(SERVER_FIFO, O_RDONLY);
            if (server_fd == -1) {
                perror("Unable to reopen server FIFO");
                exit(1);
            }
        }
    }

    close(server_fd);
    close(placeholder_fd);
    return 0;
}
