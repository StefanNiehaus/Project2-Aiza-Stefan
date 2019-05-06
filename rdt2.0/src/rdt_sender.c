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

#define RETRY 400  // timeout in milliseconds

int window_size = 1;  // CWND
int sockfd;           // UDP socket
struct sockaddr_in serveraddr;
int serverlen;  // size of serveraddr
node sndpkts_head = NULL;
node sndpkts_tail = NULL;
int ssthresh = 64;
int slow_start = 1;

// timers for selective repeat
struct itimerval timer;
sigset_t sigmask;

int remove_old_pkts(int last_byte_acked) {
  int packets_removed = 0;
  node cur = sndpkts_head;
  node next_node;
  while (1) {
    if (cur && cur->pkt->hdr.seqno < last_byte_acked &&
        cur->pkt->hdr.data_size) {
      VLOG(DEBUG, "Removing packet: %d", cur->pkt->hdr.seqno);
      next_node = cur->next;
      free(cur->pkt);
      free(cur);
      packets_removed++;
      cur = next_node;
    } else {
      break;
    }
  }
  sndpkts_head = cur;
  if (!sndpkts_head) {
    sndpkts_tail = sndpkts_head;
  }
  return packets_removed;
}

void resend_packets(int sig) {
  if (sig == SIGALRM) {
    VLOG(INFO, "Timout happend");
    ssthresh = MAX(window_size / 2, 2);
    window_size = 1;
    slow_start = 1;
    if (sendto(sockfd, sndpkts_head->pkt,
               TCP_HDR_SIZE + get_data_size(sndpkts_head->pkt), 0,
               (const struct sockaddr *)&serveraddr, serverlen) < 0) {
      error("sendto");
    }
  }
}

void start_timer() {
  VLOG(INFO, "Start timer");
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
  FILE *fp_record;

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

  // open file to record CWND size
  fp_record = fopen("CWND.csv", "w");
  if (fp_record == NULL) {
    error("Failed to open CWND.csv");
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
  int timesduplicate = 0;
  float increment_window = 0;

  while (1) {
    // Record window size
    fprintf(fp_record, "%d\n", window_size);

    VLOG(DEBUG, "Packets in flight: %d - Window Size: %d", num_pkts_sent,
         window_size);

    // Send packets within window size
    // keep all sent packets buffered in sndpkts array
    while (!done && num_pkts_sent < window_size) {
      int len = fread(buffer, 1, DATA_SIZE, fp);

      // check for EOF
      if (len <= 0) {
        VLOG(INFO, "End Of File has been reached");
        sndpkts_tail->next = create_node(make_packet(0));  // create empty pkt
        sndpkts_tail = sndpkts_tail->next;
        done = 1;
        num_pkts_sent++;
        break;
      }

      // update window
      send_base = next_seqno;
      next_seqno = send_base + len;

      // create packet
      VLOG(INFO, "Creating packet %d", send_base);
      tcp_packet *pkt = make_packet(len);
      memcpy(pkt->data, buffer, len);
      pkt->hdr.data_size = len;
      pkt->hdr.seqno = send_base;
      pkt->hdr.ackno = send_base + len;
      pkt->hdr.ctr_flags = DATA;
      if (!sndpkts_head) {
        sndpkts_head = create_node(pkt);
        sndpkts_tail = sndpkts_head;
      } else {
        // if we have a head, we have a tail!
        sndpkts_tail->next = create_node(pkt);
        sndpkts_tail = sndpkts_tail->next;
      }

      VLOG(INFO, "Sending bytes starting at %d to %s", send_base,
           inet_ntoa(serveraddr.sin_addr));

      // random initialization of port for first `sendto` call
      if (sendto(sockfd, sndpkts_tail->pkt,
                 TCP_HDR_SIZE + get_data_size(sndpkts_tail->pkt), 0,
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

    // check triple duplicate ACKs
    if (timesduplicate >= 3) {
      VLOG(INFO, "Triple ACK");
      VLOG(INFO, "Sending packet %d", sndpkts_head->pkt->hdr.ackno);
      if (sendto(sockfd, sndpkts_head->pkt,
                 TCP_HDR_SIZE + get_data_size(sndpkts_head->pkt), 0,
                 (const struct sockaddr *)&serveraddr, serverlen) < 0) {
        error("sendto");
      }
      timesduplicate = 0;
      window_size = 1;
      ssthresh = MAX(window_size / 2, 2);
      slow_start = 1;
    }

    // check cumulative ACK
    if (recvpkt->hdr.ackno > last_byte_acked) {
      timesduplicate = 0;
      last_byte_acked = recvpkt->hdr.ackno;
      stop_timer();
      printf("%s\n", "REMOVING OLD PACKETS");
      int packets_removed = remove_old_pkts(last_byte_acked);
      num_pkts_sent -= packets_removed;

      // adjust window size
      if (slow_start) {
        window_size += packets_removed;
      } else {
        increment_window += (1.0 / window_size) * (packets_removed);
        if (increment_window > 1) {
          ++window_size;
          increment_window -= 1.0;
        }
      }

      if (window_size >= ssthresh) {
        slow_start = 0;
      }

      start_timer();

      // send final packet
      if (done) {
        if (num_pkts_sent == 1) {
          if (sendto(sockfd, sndpkts_head->pkt,
                     TCP_HDR_SIZE + get_data_size(sndpkts_head->pkt), 0,
                     (const struct sockaddr *)&serveraddr, serverlen) < 0) {
            error("sendto");
          }
          exit(EXIT_SUCCESS);
        }  // TODO: workaround to double free (see line 56)
      }
    } else if (recvpkt->hdr.ackno == last_byte_acked) {  // increment dup ACK
      ++timesduplicate;
    }
  }

  return EXIT_SUCCESS;
}
