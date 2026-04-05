// Leo Deng, deng279
// Hersh, tripathh
// ...
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <poll.h>

int current_sessions = 0;

void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        fprintf(stdout, "reaped a zombie process");
        current_sessions--;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Use: musas <server ip> <server port> <ilambda> <logfile.dat>\n");
        exit(1);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    float ilambda = atof(argv[3]);
    char *logfile = argv[4];

    int listenfd;
    struct sockaddr_in address;
    listenfd = socket(PF_INDET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(server_port);
    inet_ption(AF_INET, server_ip, &address.sin_addr);

    if (bind(listenfd, (struct sockaddr *)&address, sizeoff(address)) < 0) {
        perror("Binding failed");
        exit(1);
    }

    listen(listenfd, 2);
    fprintf(stdout, "server is listening on %s:%d", server_ip, server_port);

    int current_sessions;
    int total_sessions;

    while (1) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        int connection_fd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);
        if (connection_fd < 0 || current_sessions == 2) {
            if (current_sessions == 2) {
                send(connection_fd, "q", 1, 0);
                close(connfd);
                fprintf(stdout, "rejected connection because at max sessions");
            }
            continue;
        }

        char filename_buffer[32];

    }
}
