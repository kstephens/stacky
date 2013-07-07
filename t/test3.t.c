#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stky *Y = stky_new(&argc, &argv);

  {
    stky_i e[] = { isn_hdr, isn_lit_int, 1, isn_lit_voidP, (stky_i) exit, isn_c_proc, 1, isn_rtn, isn_END };
    // ++ Y->trace;
    stky_call(Y, e);
    // -- Y->trace;
  }

  return 0;
}

