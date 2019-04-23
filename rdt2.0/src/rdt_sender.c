/*
 * Aiza Usman and Stefan Niehaus
 * NYU Abu Dhabi
 */
#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "packet.h"

#define RETRY 400  // timeout in milliseconds
#define MAX_PACKETS 100000

int window_size = 10;

int sockfd, serverlen;
struct sockaddr_in serveraddr;
tcp_packet *sndpkts[MAX_PACKETS] = {};

// timers for selective repeat
struct itimerval timer;
sigset_t sigmask;

void reorder_sndpkts() {
  size_t first_not_null_i = 0;
  for (size_t i = 0; i < MAX_PACKETS; i++) {
    if (sndpkts[i]) {
      first_not_null_i = i;
      // VLOG(DEBUG, "Start re-order from index: %d", first_not_null_i);
      break;
    }
  }

  for (size_t base = 0; base < MAX_PACKETS; base++) {
    if (sndpkts[first_not_null_i] == NULL) {
      return;
    }
    // VLOG(DEBUG, "Move %d to index %d", sndpkts[first_not_null_i]->hdr.seqno,
    // base);
    sndpkts[base] = sndpkts[first_not_null_i++];
  }
}

int remove_old_pkts(int last_byte_acked) {
  int packets_removed = 0;
  for (size_t i = 0; i < MAX_PACKETS; i++) {
    if (sndpkts[i] && sndpkts[i]->hdr.ackno <= last_byte_acked) {
      // VLOG(DEBUG, "Removing packet: %d at index %d", sndpkts[i]->hdr.seqno, i);
      free(sndpkts[i]);  // TODO: double free causes core dump when final packet is sent
      sndpkts[i] = NULL;
      packets_removed++;
    }
  }
  reorder_sndpkts();
  return packets_removed;
}

void resend_packets(int sig) {
  if (sig == SIGALRM) {
    // Resend all packets range between
    // sendBase and nextSeqNum
    VLOG(INFO, "Timout happend");
    if (sendto(sockfd, sndpkts[0], TCP_HDR_SIZE + get_data_size(sndpkts[0]), 0,
               (const struct sockaddr *)&serveraddr, serverlen) < 0) {
      error("sendto");
    }
  }
}

void start_timer() {
  sigprocmask(SIG_UNBLOCK, &sigmask, NULL);
  setitimer(ITIMER_REAL, &timer, NULL);
}

void stop_timer() { sigprocmask(SIG_BLOCK, &sigmask, NULL); }

// init_timer: Initialize timeer
// delay: delay in milli seconds
// sig_handler: signal handler function for resending unacknoledge packets
void init_timer(int delay, void (*sig_handler)(int)) {
  signal(SIGALRM, resend_packets);
  timer.it_interval.tv_sec = delay / 1000;  // sets an interval of the timer
  timer.it_interval.tv_usec = (delay % 1000) * 1000;
  timer.it_value.tv_sec = delay / 1000;  // sets an initial value
  timer.it_value.tv_usec = (delay % 1000) * 1000;

  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGALRM);
}

int main(int argc, char **argv) {
  int portno;      // port number for reciever
  char *hostname;  // address of reciever
  char buffer[DATA_SIZE];
  FILE *fp;

  // check command line arguments
  if (argc != 4) {
    fprintf(stderr, "usage: %s <hostname> <port> <FILE>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  hostname = argv[1];
  portno = atoi(argv[2]);

  // open file to send
  fp = fopen(argv[3], "r");
  if (fp == NULL) {
    error(argv[3]);
  }

  // socket: create the socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) error("ERROR opening socket");

  // initialize server server details
  bzero((char *)&serveraddr, sizeof(serveraddr));
  serverlen = sizeof(serveraddr);

  // covert host into network byte order
  if (inet_aton(hostname, &serveraddr.sin_addr) == 0) {
    fprintf(stderr, "ERROR, invalid host %s\n", hostname);
    exit(EXIT_FAILURE);
  }

  // build the server's Internet address
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(portno);

  // Stop and wait protocol
  init_timer(RETRY, resend_packets);
  int next_seqno = 0;
  int send_base = 0;
  int last_byte_acked = 0;
  int num_pkts_sent = 0;
  int done = 0;
  while (1) {
    VLOG(DEBUG, "Packets in flight: %d - Window Size: %d", num_pkts_sent,
         window_size);

    // Send packets within window size
    // keep all sent packets buffered in sndpkts array
    while (!done && num_pkts_sent < window_size) {
      int len = fread(buffer, 1, DATA_SIZE, fp);

      // check for EOF
      if (len <= 0) {
        VLOG(INFO, "End Of File has been reached");
        sndpkts[num_pkts_sent] = make_packet(0);
        sendto(sockfd, sndpkts[num_pkts_sent], TCP_HDR_SIZE, 0,
               (const struct sockaddr *)&serveraddr, serverlen);
        done = 1;
        break;
      }

      // update window
      send_base = next_seqno;
      next_seqno = send_base + len;

      // create packet
      VLOG(INFO, "Creating packet %d", send_base);
      sndpkts[num_pkts_sent] = make_packet(len);
      memcpy(sndpkts[num_pkts_sent]->data, buffer, len);
      sndpkts[num_pkts_sent]->hdr.data_size = len;
      sndpkts[num_pkts_sent]->hdr.seqno = send_base;
      sndpkts[num_pkts_sent]->hdr.ackno = send_base + len;
      sndpkts[num_pkts_sent]->hdr.ctr_flags = DATA;

      VLOG(INFO, "Sending bytes starting at %d to %s", send_base,
           inet_ntoa(serveraddr.sin_addr));

      // random initialization of port for first `sendto` call
      if (sendto(sockfd, sndpkts[num_pkts_sent],
                 TCP_HDR_SIZE + get_data_size(sndpkts[num_pkts_sent]), 0,
                 (const struct sockaddr *)&serveraddr, serverlen) < 0) {
        error("sendto");
      }
      num_pkts_sent++;

      if (num_pkts_sent == 1) {
        start_timer();
      }
    }

    // recieve a packet
    if (recvfrom(sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *)&serveraddr,
                 (socklen_t *)&serverlen) < 0) {
      error("recvfrom");
    }
    tcp_packet *recvpkt;
    recvpkt = (tcp_packet *)buffer;
    VLOG(DEBUG, "Recieved ACK: %d - Last Correct ACK: %d\n", recvpkt->hdr.ackno,
         last_byte_acked);
    assert(get_data_size(recvpkt) <= DATA_SIZE);

    // check cumulative ACK
    if (recvpkt->hdr.ackno > last_byte_acked) {
      last_byte_acked = recvpkt->hdr.ackno;
      stop_timer();
      if (done && num_pkts_sent == 1) {
        exit(EXIT_SUCCESS);
      }  // TODO: workaround to double free (see line 56)
      printf("%s\n", "REMOVING OLD PACKETS");
      int packets_removed = remove_old_pkts(last_byte_acked);
      num_pkts_sent -= packets_removed;
      if (done) {
        start_timer();
      }
    }
  }

  return 0;
}
