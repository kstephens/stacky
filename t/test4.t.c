#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stky *Y = stky_new(&argc, &argv);
  stky_io__eval(Y, Y->v_stdin);
  return 0;
}

