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

#define STDIN_FD 0
#define RETRY 400  // milli second

int next_seqno = 0;
int send_base = 0;
int window_size = 1;

int sockfd, serverlen;
struct sockaddr_in serveraddr;
tcp_packet *sndpkt;
tcp_packet *recvpkt;

struct itimerval timer;
sigset_t sigmask;

void resend_packets(int sig) {
  if (sig == SIGALRM) {
    // Resend all packets range between
    // sendBase and nextSeqNum
    VLOG(INFO, "Timout happend");
    if (sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
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

/*
 * init_timer: Initialize timeer
 * delay: delay in milli seconds
 * sig_handler: signal handler function for resending unacknoledge packets
 */
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
  int portno, len;
  int next_seqno;
  char *hostname;
  char buffer[DATA_SIZE];
  FILE *fp;

  // check command line arguments
  if (argc != 4) {
    fprintf(stderr, "usage: %s <hostname> <port> <FILE>\n", argv[0]);
    exit(0);
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
    exit(0);
  }

  // build the server's Internet address
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(portno);

  // Stop and wait protocol
  init_timer(RETRY, resend_packets);
  next_seqno = 0;
  while (1) {
    len = fread(buffer, 1, DATA_SIZE, fp);

    // check for EOF
    if (len <= 0) {
      VLOG(INFO, "End Of File has been reached");
      sndpkt = make_packet(0);
      sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0,
             (const struct sockaddr *)&serveraddr, serverlen);
      break;
    }

    // update window
    send_base = next_seqno;
    next_seqno = send_base + len;

    // create packet
    sndpkt = make_packet(len);
    memcpy(sndpkt->data, buffer, len);
    sndpkt->hdr.seqno = send_base;

    // Wait for ACK
    do {
      // VLOG(DEBUG, "Sending packet %d to %s", send_base,
      // inet_ntoa(serveraddr.sin_addr));

      // random initialization of port for first `sendto` call
      if (sendto(sockfd, sndpkt, TCP_HDR_SIZE + get_data_size(sndpkt), 0,
                 (const struct sockaddr *)&serveraddr, serverlen) < 0) {
        error("sendto");
      }

      // recieve ACK
      start_timer();
      if (recvfrom(sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *)&serveraddr,
                   (socklen_t *)&serverlen) < 0) {
        error("recvfrom");
      }

      // check ACK
      recvpkt = (tcp_packet *)buffer;
      printf("%d \n", get_data_size(recvpkt));
      assert(get_data_size(recvpkt) <= DATA_SIZE);

      stop_timer();

      // resend pack if dont recv ack
    } while (recvpkt->hdr.ackno != next_seqno);

    free(sndpkt);
  }

  return 0;
}