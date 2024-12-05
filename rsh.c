#include <stdio.h>
#include <stdlib.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <pthread.h>

#define N 13

extern char **environ;
char uName[20];

char *allowed[N] = {"cp","touch","mkdir","ls","pwd","cat","grep","chmod","diff","cd","exit","help","sendmsg"};

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

int isAllowed(char *cmd) {
    for (int i = 0; i < N; i++) {
        if (strcmp(cmd, allowed[i]) == 0)
            return 1;
    }
    return 0;
}

void sendmsg(char *user, char *target, char *msg) {
    struct message req;
    int server_fd;

    strcpy(req.source, user);
    strcpy(req.target, target);
    strcpy(req.msg, msg);

    server_fd = open("serverFIFO", O_WRONLY);
    if (server_fd < 0) {
        perror("Can't open server FIFO");
        return;
    }

    if (write(server_fd, &req, sizeof(req)) < 0) {
        perror("Can't write to server FIFO");
    }

    close(server_fd);
}

void* messageListener(void *arg) {
    char fifo_name[256];
    struct message incoming_msg;
    int user_fifo_fd;

    snprintf(fifo_name, sizeof(fifo_name), "%s", (char *)arg);

    while (1) {
        user_fifo_fd = open(fifo_name, O_RDONLY);
        if (user_fifo_fd < 0) {
            perror("Can't open user FIFO");
            sleep(1);
            continue;
        }

        while (1) {
            ssize_t bytes_read = read(user_fifo_fd, &incoming_msg, sizeof(incoming_msg));
            if (bytes_read > 0) {
                printf("\nIncoming message from %s: %s\n", incoming_msg.source, incoming_msg.msg);
                fflush(stdout);
                fprintf(stderr, "rsh> ");  // Reprint prompt after message
                fflush(stderr);
            } else if (bytes_read == 0) {
                // Writing end is closed, reopen FIFO
                close(user_fifo_fd);
                break;
            } else {
                perror("Error reading from FIFO");
            }
        }
    }

    pthread_exit(NULL);
}

int main(int argc, char **argv) {
    pthread_t listener_thread;
    char fifo_name[256];
    char line[256];

    if (argc != 2) {
        printf("Usage: ./rsh <username>\n");
        exit(1);
    }

    signal(SIGINT, terminate);

    strcpy(uName, argv[1]);
    snprintf(fifo_name, sizeof(fifo_name), "%s", uName);


    if (pthread_create(&listener_thread, NULL, messageListener, fifo_name) != 0) {
        perror("Can't create listener thread");
        unlink(fifo_name);
        exit(1);
    }

    while (1) {
        fprintf(stderr, "rsh> ");
        if (fgets(line, sizeof(line), stdin) == NULL) continue;

        line[strcspn(line, "\n")] = '\0';  // Remove newline character
        if (strlen(line) == 0) continue;

        char *cmd = strtok(line, " ");
        if (!isAllowed(cmd)) {
            printf("Not allowed!\n");
            continue;
        }

        if (strcmp(cmd, "sendmsg") == 0) {
            char *target = strtok(NULL, " ");
            char *msg = strtok(NULL, "");
            if (!target) {
                printf("sendmsg: specify target user\n");
            } else if (!msg) {
                printf("sendmsg: enter a message\n");
            } else {
                sendmsg(uName, target, msg);
            }
            continue;
        }

        if (strcmp(cmd, "exit") == 0) {
            break;
        }

        // Handle other allowed commands
        char *args[20];
        int i = 0;
        args[i++] = cmd;
        char *token;
        while ((token = strtok(NULL, " ")) != NULL && i < 19) {
            args[i++] = token;
        }
        args[i] = NULL;

        pid_t pid;
        int status;
        if (posix_spawn(&pid, cmd, NULL, NULL, args, environ) == 0) {
            waitpid(pid, &status, 0);
        } else {
            perror("Failed to execute command");
        }
    }

    pthread_cancel(listener_thread);
    pthread_join(listener_thread, NULL);
    unlink(fifo_name);
    return 0;
}
