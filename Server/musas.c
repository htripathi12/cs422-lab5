// Leo Deng, deng279

// Hersh

// Louis Nguyen, nguye576

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

#include <signal.h>


int current_sessions = 0;



void sigchld_handler(int sig) {

    (void)sig;

    while (waitpid(-1, NULL, WNOHANG) > 0) {

        fprintf(stdout, "reaped a zombie process");

        current_sessions--;

    }

}

// Function for checking filename - Louis
static int check_filename(const char *s) {

    int n = (int)strlen(s);
    if (n < 4) {
      return 0;
    }

    // must end with .au
    if (s[n - 3] != '.' || s[n - 2] != 'a' || s[n - 1] != 'u') {
      return 0;
    }

   // name part must be < 12
    if (n - 3 > 12) {
      return 0;
    }

    // name part must be lowercase or numbers only
    for (int i = 0; i < n - 3; i++) {
      int is_lowercase = (s[i] >= 'a' && s[i] <= 'z');
      int is_number = (s[i] >=0 && s[i] <= '9');

      if (is_lowercase == 0 && is_number == 0) {
        return 0;
      }
    }

    return 1;

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

    signal(SIGCHLD, sigchld_handler);



    int listenfd;

    struct sockaddr_in address;

    listenfd = socket(PF_INET, SOCK_STREAM, 0);



    int opt = 1;

    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));



    memset(&address, 0, sizeof(address));

    address.sin_family = AF_INET;

    address.sin_port = htons(server_port);

    inet_pton(AF_INET, server_ip, &address.sin_addr);



    if (bind(listenfd, (struct sockaddr *)&address, sizeof(address)) < 0) {

        perror("Binding failed");

        exit(1);

    }



    listen(listenfd, 2);

    fprintf(stdout, "server is listening on %s:%d\n", server_ip, server_port);



    // int current_sessions;

   //  int total_sessions;



    while (1) {

        struct sockaddr_in client_addr;

        socklen_t client_len = sizeof(client_addr);

        int connection_fd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);

        // Might be incorrect since you're sending even if accept < 0
        /*
          if (connection_fd < 0 || current_sessions == 2) {

            if (current_sessions == 2) {

                send(connection_fd, "q", 1, 0);

                close(connfd);

                fprintf(stdout, "rejected connection because at max sessions");

            }

            continue;

        }
       */

        if (connection_fd < 0) {
          perror("accept");
          continue;
        }

        if (current_sessions >= 2) {
          send(connection_fd, "q", 1, 0);
          close(connection_fd);
          fprintf(stdout, "Rejected connection because at max sessions\n");
          continue;
        }

        char filename_buffer[32];
        memset(filename_buffer, 0, sizeof(filename_buffer));

        ssize_t r = recv(connection_fd, filename_buffer, 31, 0);
        if (r < 0) {
          close(connection_fd);
          continue;
        }

        // Get rid of '\n' if there's any
        if (filename_buffer[r - 1] == '\n') {
          filename_buffer[r - 1] = '\0';
        }

        if (check_filename(filename_buffer) == 0) {
          send(connection_fd, "q", 1, 0);
          close(connection_fd);
          fprintf(stdout, "Invalid filename, sent q! \n");
        }

        pid_t pid = fork();
        if (pid < 0) {
          perror("fork");
          send(connection_fd, "q", 1, 0);
          close(connection_fd);
          continue;
        }

        if (pid > 0) {
          // parent
          current_sessions++;
          send(connection_fd, "K", 1, 0);
          close(connection_fd);
          fprintf(stdout, "Accepted session for %s (sessions = %d)\n", filename_buffer, current_sessions);
          continue;
        }

        // child
        uint16_t client_udp_net;
        ssize_t r2 = recv(connection_fd, &client_udp_net, 2, 0);
        if (r2 != 2) {
          close(connection_fd);
          exit(1);
        }
        uint16_t client_udp_port = ntohs(client_udp_net);

        // streaming handle later, just print out to check for now
        fprintf(stdout, "Child got client UDP port %u for file %s\n", client_udp_port, filename_buffer);
        close(connection_fd);
        exit(0);

    }

}
