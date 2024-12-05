
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

struct message {
	char source[50];
	char target[50]; 
	char msg[200]; // message body
};

void terminate(int sig) {
	printf("Exiting....\n");
	fflush(stdout);
	exit(0);
}

int main() {
    int server, target, dummyfd;
    struct message req;

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, terminate);



    // Open server FIFO for reading
    server = open("serverFIFO", O_RDONLY);
    //if (server < 0) {
        //perror("Can't open server FIFO");
      //  exit(EXIT_FAILURE);
    //}

    // Dummy FD to keep FIFO open
    dummyfd = open("serverFIFO", O_WRONLY);
    //if (dummyfd < 0) {
      //  perror("Can't open dummy FD");
      //  close(server);
       // exit(EXIT_FAILURE);
   // }

    while (1) {
        // Read a message from server FIFO
        ssize_t bytes_read = read(server, &req, sizeof(struct message));
        if (bytes_read <= 0) continue;

        printf("Received a request from %s to send the message %s to %s.\n",
               req.source, req.msg, req.target);

        // Open target's FIFO to forward message
        target = open(req.target, O_WRONLY);
        if (target == -1) {
            printf("Can't open target FIFO");
            continue;
        }

        // Write message to target FIFO
        if (write(target, &req, sizeof(req)) < 0) {
            printf("Can't write to target FIFO");
        }

        close(target);
    }

    // Cleanup (though this code is unreachable due to infinite loop)
    close(server);
    close(dummyfd);

    return 0;
}
