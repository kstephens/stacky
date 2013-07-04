#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stky *Y = stky_new();
  FILE *in = stdin;
  while ( ! feof(in) ) {
    stky_repl(Y, in, stdout);
  }
  return 0;
}

