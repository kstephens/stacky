#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stacky *Y = stacky_new();
  {
    word_t e[] = { isn_hdr,
                   isn_lint, 1, isn_lint, 1,
                   isn_ident_charP, (word_t) "%eq", isn_lookup, isn_call,
                   isn_write_int, isn_lcharP, (word_t) "\n", isn_write_charP,
                   isn_rtn, isn_END, };
    ++ Y->trace;
    stacky_call(Y, e);
    -- Y->trace;
  }

  return 0;
}
