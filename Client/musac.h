#ifndef MUSAC_H
#define MUSAC_H

#include <sys/time.h>
#include <stdio.h>

typedef struct {
  char **audio_buffer;
  int num_packets;
  int write_pos;
  int read_pos;
  int packets_in_buffer;
  double *Q_log;
  double *time_log;
  int log_count;
  int log_len;
  FILE *logfp;
  struct timeval start_tv;
} buffer_state_t;

#endif
