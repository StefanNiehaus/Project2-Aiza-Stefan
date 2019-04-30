/*
 * Aiza Usman and Stefan Niehaus
 * NYU Abu Dhabi
 */
#include <stdio.h>
#include <stdlib.h>

#include "common.h"

int verbose = ALL;

// error - wrapper for perror
void error(char *msg) {
  perror(msg);
  exit(1);
}
