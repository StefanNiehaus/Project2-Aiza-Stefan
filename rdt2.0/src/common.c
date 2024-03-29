/*
 * Aiza Usman and Stefan Niehaus
 * NYU Abu Dhabi
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int verbose = ALL;

// creates empty node to add to linked list
// that serves as buffer of sent packets
node create_node(tcp_packet *pkt) {
  node temp = (node)malloc(sizeof(struct linked_list));
  temp->pkt = pkt;
  temp->next = NULL;
  return temp;
}

// error - wrapper for perror
void error(char *msg) {
  perror(msg);
  exit(EXIT_FAILURE);
}
