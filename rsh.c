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
    pid_t pid;
    char **cargv;
    char *path;
    int status;
    posix_spawnattr_t attr;
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

        if (strcmp(line,"\n") == 0) continue;

        line[strlen(line)-1] = '\0';  // Remove newline character
        
        char cmd[256];
        char line2[256];
        strcpy(line2,line);
        strcpy(cmd,strtok(line," "));

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

        if (strcmp(cmd,"cd")==0) {
            char *targetDir=strtok(NULL," ");
            if (strtok(NULL," ")!=NULL) {
                printf("-rsh: cd: too many arguments\n");
            }
            else {
                chdir(targetDir);
            }
            continue;
        }

        if (strcmp(cmd,"help")==0) {
            printf("The allowed commands are:\n");
            for (int i=0;i<N;i++) {
                printf("%d: %s\n",i+1,allowed[i]);
            }
            continue;
        }

        cargv = (char**)malloc(sizeof(char*));
        cargv[0] = (char *)malloc(strlen(cmd)+1);
        path = (char *)malloc(9+strlen(cmd)+1);
        strcpy(path,cmd);
        strcpy(cargv[0],cmd);

        char *attrToken = strtok(line2," "); /* skip cargv[0] which is completed already */
        attrToken = strtok(NULL, " ");
        int n = 1;
        while (attrToken!=NULL) {
            n++;
            cargv = (char**)realloc(cargv,sizeof(char*)*n);
            cargv[n-1] = (char *)malloc(strlen(attrToken)+1);
            strcpy(cargv[n-1],attrToken);
            attrToken = strtok(NULL, " ");
        }
        cargv = (char**)realloc(cargv,sizeof(char*)*(n+1));
        cargv[n] = NULL;

        // Initialize spawn attributes
        posix_spawnattr_init(&attr);

        // Spawn a new process
        if (posix_spawnp(&pid, path, NULL, &attr, cargv, environ) != 0) {
            perror("spawn failed");
            exit(EXIT_FAILURE);
        }

        // Wait for the spawned process to terminate
        if (waitpid(pid, &status, 0) == -1) {
            perror("waitpid failed");
            exit(EXIT_FAILURE);
        }

        // Destroy spawn attributes
        posix_spawnattr_destroy(&attr);

        }
    return 0;
}
