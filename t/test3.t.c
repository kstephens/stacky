#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stacky *Y = stacky_new();

  {
    word_t e[] = { isn_hdr, isn_lint, 1, isn_lvoidP, (word_t) exit, isn_c_proc, 1, isn_rtn, isn_END };
    // ++ Y->trace;
    stacky_call(Y, e);
    // -- Y->trace;
  }

  return 0;
}

