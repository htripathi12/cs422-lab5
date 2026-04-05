// musac.c
// Leo Deng -
// Louis Nguyen - nguye576
// Hersh -

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void usage(const char *s) {
  printf("Usage: %s <server_ip> <server_port> <filename.au>\n", s);
  exit(1);
}

int main(int argc, char **argv) {
  if (argc != 4) {
    usage(argv[0]);
  }

  const char *server_ip = argv[1];
  int server_port = atoi(argv[2]);
  const char *filename = argv[3];

  // TCP connect to server
  int tcpsock = socket(AF_INET, SOCK_STREAM, 0);
  if (tcpsock < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in server;
  memset(&server, 0, sizeof(server));
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)server_port);
  if (inet_pton(AF_INET, server_ip, &server.sin_addr) != 1) {
    printf("Bad server IP!!!\n");
    return 1;
  }
  
  if (connect(tcpsock, (struct sockaddr *)&server, sizeof(server)) < 0) {
    perror("connect");
    return 1;
  }

  //send filename to server
  send(tcpsock, filename, strlen(filename), 0);

  char response;
  ssize_t r = recv(tcpsock, &response, 1, 0);

  if (r != 1) {
    perror("recv(response)");
    close(tcpsock);
    return 1;
  }

  if (response == 'q') {
    printf("Server rejected request!\n");
    close(tcpsock);
    return 0;
  }

  if (response != 'K') {
    printf("Unexpected server response: %c\n", response);
    close(tcpsock);
    return 1;
  }

  printf("Server accpeted (K). Binding UDP port...\n");
  
  // Create UDP socket and bind from 51234
  int udpsock = socket(AF_INET, SOCK_DGRAM, 0);
  if (udpsock < 0) {
    perror("uydp socket");
    close(tcpsock);
    return 1;
  }

  struct sockaddr_in to_bind;
  memset(&to_bind, 0, sizeof(to_bind));
  to_bind.sin_family = AF_INET;
  to_bind.sin_addr.s_addr = htonl(INADDR_ANY);

  uint16_t chosen_port = 0;
  for (int p = 51234; p <= 65535; p++) {
    to_bind.sin_port = htons((uint16_t)p);
    if (bind(udpsock, (struct sockaddr *)&to_bind, sizeof(to_bind)) == 0) {
      chosen_port = (uint16_t)p;
      break;
    }
  }

  if (chosen_port == 0) {
    printf("Could not bind any UDP port starting from 51234\n");
    close(udpsock);
    close(tcpsock);
    return 1;
  }

  printf("Client UDP port = %u\n", chosen_port);

  uint16_t port_net = htons(chosen_port);
  // send UDP port to server over TCP as 2 bytes
  if (send(tcpsock, &port_net, 2, 0) != 2) {
    perror("send(port)");
  }

  close(udpsock);
  close(tcpsock);
  return 0;
}


