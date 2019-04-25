/*
 * Aiza Usman and Stefan Niehaus
 * NYU Abu Dhabi
 */

#include <arpa/inet.h>
#include <assert.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "packet.h"

tcp_packet *recvpkt;
tcp_packet *sndpkt;

int main(int argc, char **argv) {
  int sockfd;                     // socket
  int portno;                     // port to listen on
  struct sockaddr_in serveraddr;  // server addr
  struct sockaddr_in clientaddr;  // client addr
  FILE *fp;
  char buffer[MSS_SIZE];
  struct timeval tp;
  int expected_seqno = 0;

  // check command line arguments
  if (argc != 3) {
    fprintf(stderr, "usage: %s <port> FILE_RECVD\n", argv[0]);
    exit(1);
  }
  portno = atoi(argv[1]);

  // Open file to write
  fp = fopen(argv[2], "w");
  if (fp == NULL) {
    error(argv[2]);
  }

  // socket: create the parent socket
  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) error("ERROR opening socket");

  // Handy debugging trick to rerun the server immediately after we kill it;
  // otherwise we have to wait about 20 secs.
  // Eliminates "ERROR on binding: Address already in use" error.
  int optval = 1;
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
             sizeof(int));

  // build the server's Internet address
  bzero((char *)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)portno);

  // bind: associate the parent socket with a port
  if (bind(sockfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0)
    error("ERROR on binding");

  // main loop: wait for a datagram, then echo it
  VLOG(DEBUG, "epoch time, bytes received, sequence number");

  int clientlen = sizeof(clientaddr);  // byte size of client's address
  while (1) {
    // recvfrom: receive a UDP datagram from a client
    // VLOG(DEBUG, "waiting from server \n");
    if (recvfrom(sockfd, buffer, MSS_SIZE, 0, (struct sockaddr *)&clientaddr,
                 (socklen_t *)&clientlen) < 0) {
      error("ERROR in recvfrom");
    }

    // check data size of recieved packet
    recvpkt = (tcp_packet *)buffer;
    assert(get_data_size(recvpkt) <= DATA_SIZE);

    // check for end of file
    if (recvpkt->hdr.data_size == 0) {
      fclose(fp);
      break;
    }

    // sendto: ACK back to the client
    gettimeofday(&tp, NULL);
    VLOG(DEBUG, "Time: %lu, Data Size: %d, Seqno: %d", tp.tv_sec,
         recvpkt->hdr.data_size, recvpkt->hdr.seqno);

    // send file pointer to beginning of file and write data
    if (recvpkt->hdr.seqno == expected_seqno) {
      fseek(fp, recvpkt->hdr.seqno, SEEK_SET);
      fwrite(recvpkt->data, 1, recvpkt->hdr.data_size, fp);
      expected_seqno = recvpkt->hdr.seqno + recvpkt->hdr.data_size;
    }

    // ACK recieved packet
    sndpkt = make_packet(0);
    sndpkt->hdr.ackno = expected_seqno;
    sndpkt->hdr.ctr_flags = ACK;
    if (sendto(sockfd, sndpkt, TCP_HDR_SIZE, 0, (struct sockaddr *)&clientaddr,
               clientlen) < 0) {
      error("ERROR in sendto");
    }
  }

  return 0;
}
