#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stky *Y = stky_new(&argc, &argv);
  FILE *in = stdin;
  while ( ! feof(Y->v_stdin->opaque) ) {
    stky_repl(Y, Y->v_stdin, Y->v_stdout);
  }
  return 0;
}

