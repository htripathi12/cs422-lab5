// musac.c
// Leo Deng - deng279
// Louis Nguyen - nguye576
// Hersh - tripathh

#include <arpa/inet.h>
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include "musac.h"

/* -----------------------------------*/
/*    BELOW COPIED FROM muaudio.c     */
/* -----------------------------------*/
extern void mulawopen(size_t *bufsiz);
extern void mulawwrite(char *x);
extern void mulawclose(void);

static snd_pcm_t *mulawdev;
static snd_pcm_uframes_t mulawfrms;

// Initialize audio device.
void mulawopen(size_t *bufsiz) {
	snd_pcm_hw_params_t *p;
	unsigned int rate = 8000;

	snd_pcm_open(&mulawdev, "default", SND_PCM_STREAM_PLAYBACK, 0);
	snd_pcm_hw_params_alloca(&p);
	snd_pcm_hw_params_any(mulawdev, p);
	snd_pcm_hw_params_set_access(mulawdev, p, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(mulawdev, p, SND_PCM_FORMAT_MU_LAW);
	snd_pcm_hw_params_set_channels(mulawdev, p, 1);
	snd_pcm_hw_params_set_rate_near(mulawdev, p, &rate, 0);
	snd_pcm_hw_params(mulawdev, p);
	snd_pcm_hw_params_get_period_size(p, &mulawfrms, 0);
	*bufsiz = (size_t)mulawfrms;
	return;
}

// Write to audio device.
#define mulawwrite(x) snd_pcm_writei(mulawdev, x, mulawfrms)

// Close audio device.
void mulawclose(void) {
	snd_pcm_drain(mulawdev);
	snd_pcm_close(mulawdev);
}
/* -----------------------------------*/
/*    ABOVE COPIED FROM muaudio.c     */
/* -----------------------------------*/

static int push_audio_packet(buffer_state_t *state, const char *data) {
  if (state->packets_in_buffer >= state->num_packets) {
      fprintf(stderr, "Buffer overflow! dropping packet\n");
      return 0;
  }

  memcpy(state->audio_buffer[state->write_pos], data, 4096);
  state->write_pos = (state->write_pos + 1) % state->num_packets;
  state->packets_in_buffer++;

  return 1;
}

static char* pop_audio_packet(buffer_state_t *state) {
  if (state->packets_in_buffer <= 0) {
    return NULL;
  }

  char *block = state->audio_buffer[state->read_pos];
  state->read_pos = (state->read_pos + 1) % state->num_packets;
  state->packets_in_buffer--;

  return block;
}

static void record_trace(buffer_state_t *state, int current_Q) {
  if (state->log_count >= state->log_len) {
    return;
  }

  struct timeval now;
  gettimeofday(&now, NULL);
  double elapsed = (now.tv_sec - state->start_tv.tv_sec) * 1000.0 + 
                   (now.tv_usec - state->start_tv.tv_usec) / 1000.0;
  state->time_log[state->log_count] = elapsed;
  state->Q_log[state->log_count] = (double) current_Q;
  state->log_count++;
}

static int validate_audiofile(const char *s) {
  int n = strlen(s);
  
  // filename length 
  if (n < 4 || n - 3 > 12) {
    return 0;
  }

  // must end with .au
  if (s[n - 3] != '.' || s[n - 2] != 'a' || s[n - 1] != 'u') {
    return 0;
  }

  // filename part must be lowercase alphanumeric only
  for (int i = 0; i < n - 3; ++i) {
    int is_lowercase = (s[i] >= 'a' && s[i] <= 'z');
    int is_digit = (s[i] >= '0' && s[i] <= '9');
    
    if (!is_lowercase && !is_digit) {
      return 0;
    }
  }

  return 1;
}

static void usage(const char *s) {
  printf("Usage: %s <audiofile> <server-ip> <server-port> <igamma> <bufsize> <targetbf> <ctrace.dat>\n", s);
  exit(1);
}

static int load_control_params(float *ilambda, float *epsilon, float *beta) {
  FILE *control_fp = fopen("control-param.dat", "r");
  if (control_fp == NULL) {
    perror("fopen");
    return 0;
  }
  
  if (fscanf(control_fp, "%f %f %f", ilambda, epsilon, beta) != 3) {
    fprintf(stderr, "control-param.dat must contain three floats\n");
    fclose(control_fp);
    return 0;
  }

  fclose(control_fp);
  return 1;
}

static float compute_updated_ilambda(
  float ilambda,
  float igamma,
  int Q,
  int targetbf,
  float epsilon,
  float beta
) {
  float term1 = ((float) Q - (float) targetbf) * epsilon;
  float term2 = (igamma - ilambda) * beta;
  float new_ilambda = ilambda + term1 + term2;
  
  if (new_ilambda < 0.001) {
    new_ilambda = 0.001;
  }

  return new_ilambda;
}

int main(int argc, char **argv) {
  if (argc != 8) {
    usage(argv[0]);
  }

  const char *audiofile = argv[1];
  const char *server_ip = argv[2];
  int server_port = atoi(argv[3]);
  float igamma = atof(argv[4]);
  int bufsize = atoi(argv[5]);
  int targetbf = atoi(argv[6]);
  const char *ctrace_file = argv[7];

  if (!validate_audiofile(audiofile)) {
    fprintf(stderr, "audiofile must be < 12 lowercase alphanumeric chars with .au extension\n");
    return 1;
  }

  float ilambda, epsilon, beta;
  if (!load_control_params(&ilambda, &epsilon, &beta)) {
    return 1;
  }

  if (igamma <= 0) {
    fprintf(stderr, "igamma must be positive\n");
    return 1;
  }

  if (server_port <= 0 || server_port > 65535) {
    fprintf(stderr, "server-port must be between 1 and 65535\n");
    return 1;
  }

  if (bufsize <= 0 || bufsize % 4096 != 0) {
    fprintf(stderr, "bufsize error\n");
    return 1;
  }

  if (targetbf < 0 || targetbf > bufsize || targetbf % 4096 != 0) {
    fprintf(stderr, "targetbf error\n");
    return 1;
  }

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

  // request audiofile from server
  send(tcpsock, audiofile, strlen(audiofile), 0);

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

  // Initialize buffer state
  buffer_state_t state;
  state.num_packets = bufsize / 4096;
  state.write_pos = 0;
  state.read_pos = 0;
  state.packets_in_buffer = 0;
  state.audio_buffer = malloc(sizeof(char*) * state.num_packets);
  for (int i = 0; i < state.num_packets; i++) {
    state.audio_buffer[i] = malloc(4096);
  }
  state.log_len = 100000;
  state.Q_log = malloc(sizeof(double) * state.log_len);
  state.time_log = malloc(sizeof(double) * state.log_len);
  state.log_count = 0;
  gettimeofday(&state.start_tv, NULL);

  // Initialize the audio device before receiving packets
  size_t device_bufsiz;
  mulawopen(&device_bufsiz);

  struct timeval next_playback_time;
  gettimeofday(&next_playback_time, NULL);

  struct pollfd poll_fds[2];
  poll_fds[0].fd = udpsock;
  poll_fds[0].events = POLLIN;
  poll_fds[1].fd = tcpsock;
  poll_fds[1].events = POLLIN;

  struct sockaddr_in server_udp_addr;
  socklen_t addr_len = sizeof(server_udp_addr);

  int got_server_address = 0;
  int streaming_done = 0;
  int bytes_received = 0;
  while (!streaming_done || state.packets_in_buffer > 0) {
    // finish all waiting udp before playing
    while (1) {
        int has_udp = poll(poll_fds, 1, 0) > 0 && (poll_fds[0].revents & POLLIN);
        if (!has_udp) {
          break;
        }
        char packet[4096];
        ssize_t received = recvfrom(udpsock, packet, 4096, 0, (struct sockaddr *)&server_udp_addr, &addr_len);
        if (received > 0) {
            push_audio_packet(&state, packet);
            bytes_received += received;
            got_server_address = 1;
            int current_Q = state.packets_in_buffer * 4096;
            ilambda = compute_updated_ilambda(ilambda, igamma, current_Q, targetbf, epsilon, beta);
            sendto(udpsock, &ilambda, sizeof(float), 0, (struct sockaddr *)&server_udp_addr, addr_len);
        }
    }

    // Check for termination signal from server over TCP
    int tcp_ready = poll(&poll_fds[1], 1, 0) > 0 && (poll_fds[1].revents & POLLIN);
    if (tcp_ready) {
        char signal;
        if (recv(tcpsock, &signal, 1, 0) > 0 && signal == 'Q') {
            streaming_done = 1;
            printf("Server finished, bytes received: %d\n", bytes_received);
        }
    }
    // Get current time and check if it's time to play the next audio block
    struct timeval now;
    gettimeofday(&now, NULL);

    // Pop and play the next audio packet from buffer
    char *block = pop_audio_packet(&state);
    if (block) {
        mulawwrite(block);
        int current_Q = state.packets_in_buffer * 4096;
        ilambda = compute_updated_ilambda(ilambda, igamma, current_Q, targetbf, epsilon, beta);
        if (got_server_address) {
            sendto(udpsock, &ilambda, sizeof(float), 0, (struct sockaddr *)&server_udp_addr, addr_len);
        }
        record_trace(&state, current_Q);
        struct timespec ts;
        ts.tv_sec = (long)igamma;
        ts.tv_nsec = (long)((igamma - ts.tv_sec) * 1e9);
        nanosleep(&ts, NULL);
    } else {
        fprintf(stderr, "Buffer Underrun!\n");
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 10000000;
        nanosleep(&ts, NULL);
    }

      // Schedule the next playback time: add igamma (playback interval) to current schedule
      
      // double next_val = next_sec + igamma;
      // next_playback_time.tv_sec = (long)next_val;
      // next_playback_time.tv_usec = (long)((next_val - (long)next_val) * 1000000);

      // // Small sleep (5ms) to prevent tight polling loop
      // struct timespec ts = {0, 5000000}; 
      // nanosleep(&ts, NULL);
  }
  

  mulawclose();

  // Write trace data
  FILE *fp = fopen(ctrace_file, "w"); 
  if (fp) {
    for (int i = 0; i < state.log_count; i++) {
      fprintf(fp, "%.3f %.0f\n", state.time_log[i], state.Q_log[i]);
    }
    fclose(fp);
    printf("Saved trace to %s\n", ctrace_file);
  } else {
    perror("Failed to open log file");
  }


  // Cleanup
  for (int i = 0; i < state.num_packets; i++) {
    free(state.audio_buffer[i]);
  }
  free(state.audio_buffer);
  free(state.Q_log);
  free(state.time_log);
  close(udpsock);
  close(tcpsock);
  return 0;
}
