#ifndef MUSAC_H
#define MUSAC_H

#include <sys/time.h>
#include <stdio.h>

typedef struct {
  char **audio_buffer; // Circular buffer storing audio packets
  int num_packets; // Max # of packets buffer can hold
  int write_pos; // Index of new packet(s) written into buffer
  int read_pos; // Index of where packets are read from buffer
  int packets_in_buffer; // Current number of packets stored in the buffer (Q)
  double *Q_log; // Array storing buffer size (Q) at each playback event
  double *time_log; // Array storing elapsed time (ms) at each playback event
  int log_count; // Number of events logged so far
  int log_len; // Maximum capacity of the log arrays
  struct timeval start_tv; // Start time used to calculate elapsed time for traces
} buffer_state_t;

#endif
