/*
 * Aiza Usman and Stefan Niehaus
 * NYU Abu Dhabi
 */
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

int verbose = ALL;

// error - wrapper for perror
void error(char *msg) {
  perror(msg);
  exit(1);
}
