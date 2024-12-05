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

void terminate(int sig) {
    printf("Exiting....\n");
    fflush(stdout);
    exit(0);
}

int main() {
    int server, dummy;
    struct message req;
    setvbuf(stdout, NULL, _IONBF, 0);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);

    server = open(SERVER_FIFO, O_RDONLY);
    if (server < 0) {
        perror("Can't create server FIFO");
        exit(1);
    }

    dummy = open(SERVER_FIFO, O_WRONLY);
    if (dummy < 0) {
        perror("Can't open dummy FD");
        close(server);
        exit(1);
    }

    while (1) {
        ssize_t bytes_read = read(server, &req, sizeof(req));
        if (bytes_read == sizeof(req)) {
            if (strlen(req.source) == 0 || strlen(req.target) == 0 || strlen(req.msg) == 0) {
                continue;
            }

            printf("Received a request from %s to send the message %s to %s.\n",
                   req.source, req.msg, req.target);

            int target = open(req.target, O_WRONLY);
            if (target < 0) {
                printf("Can't open target FIFO for %s.\n", req.target);
                continue;
            }

            ssize_t bytes_written = write(target, &req, size(req));
            if (bytes_written < 0) {
                perror("Can't write to target FIFO");
            }

            close(target);
        } else if (bytes_read < 0) {
            perror("Can't reopen server FIFO");
            fflush(stdout);
        }
    }
    close(server);
    close(dummy);
    return 0;
}
