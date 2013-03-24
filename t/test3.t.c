#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stacky *Y = stacky_new();

  {
    stky_i e[] = { isn_hdr, isn_lit_int, 1, isn_lit_voidP, (stky_i) exit, isn_c_proc, 1, isn_rtn, isn_END };
    // ++ Y->trace;
    stacky_call(Y, e);
    // -- Y->trace;
  }

  return 0;
}

