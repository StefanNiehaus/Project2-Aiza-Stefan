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

node cache_head = NULL;
int expected_seqno = 0;

void add_to_cache(tcp_packet *orig_pkt) {
  // deep copy of packet
  tcp_packet *pkt = make_packet(orig_pkt->hdr.data_size);
  memcpy(pkt->data, orig_pkt->data, orig_pkt->hdr.data_size);
  pkt->hdr.data_size = orig_pkt->hdr.data_size;
  pkt->hdr.seqno = orig_pkt->hdr.seqno;
  pkt->hdr.ackno = orig_pkt->hdr.ackno;
  pkt->hdr.ctr_flags = orig_pkt->hdr.ctr_flags;

  // create node using copied pkt
  node new_node = create_node(pkt);

  // logic to add packets
  if (!cache_head) {
    cache_head = new_node;
    return;
  } else if (new_node->pkt->hdr.seqno < cache_head->pkt->hdr.seqno) {
    new_node->next = cache_head;
    cache_head = new_node;
    return;
  }
  node cur_node = cache_head;
  while (cur_node->next &&
         cur_node->next->pkt->hdr.seqno < new_node->pkt->hdr.seqno) {
    cur_node = cur_node->next;
    if (cur_node->pkt->hdr.seqno == new_node->pkt->hdr.seqno) {
      break;
    }
  }
  node next_node = cur_node->next;
  cur_node->next = new_node;
  new_node->next = next_node;
}

node write_from_cache(FILE *fp) {
  node cur_node = cache_head;
  while (cur_node && cur_node->pkt->hdr.seqno == expected_seqno) {
    // VLOG(DEBUG, "Writing packet: %d", cur_node->pkt->hdr.seqno);
    fseek(fp, cur_node->pkt->hdr.seqno, SEEK_SET);
    fwrite(cur_node->pkt->data, 1, cur_node->pkt->hdr.data_size, fp);
    expected_seqno = cur_node->pkt->hdr.seqno + cur_node->pkt->hdr.data_size;
    node next_node = cur_node->next;
    free(cur_node);
    cur_node = next_node;
  }
  cache_head = cur_node;
  // VLOG(DEBUG, "Finished writing most recent packets to file");
  return cache_head;
}

int main(int argc, char **argv) {
  tcp_packet *recvpkt;
  tcp_packet *sndpkt;             // pkt for ACK
  int sockfd;                     // socket
  int portno;                     // port to listen on
  struct sockaddr_in serveraddr;  // server addr
  struct sockaddr_in clientaddr;  // client addr
  FILE *fp;                       // file pointer to write to
  char buffer[MSS_SIZE];          // buffer for incoming data stream
  struct timeval tp;

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
  VLOG(INFO, "epoch time, bytes received, sequence number");

  int clientlen = sizeof(clientaddr);  // byte size of client's address
  while (1) {
    // recvfrom: receive a UDP datagram from a client
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
    VLOG(INFO, "%lu.%06lu, %d, %d", tp.tv_sec, tp.tv_usec,
         recvpkt->hdr.data_size, recvpkt->hdr.seqno);

    // send file pointer to beginning of file and write data
    if (recvpkt->hdr.seqno > expected_seqno) {
      add_to_cache(recvpkt);
    } else if (recvpkt->hdr.seqno == expected_seqno) {
      add_to_cache(recvpkt);
      write_from_cache(fp);
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

  return EXIT_SUCCESS;
}
