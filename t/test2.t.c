#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stacky *Y = stacky_new();
  {
    char eq1[] = "%eq";
    char eq2[] = "%eq";
    char eq3[] = "%eq";
    word_t e[] = { isn_hdr,
                   isn_lcharP, (word_t) "@%eq @%eq %eq = ", isn_v_stdout, isn_write_charP,
                   isn_ident_charP, (word_t) eq1,
                   isn_ident_charP, (word_t) eq2,
                   isn_ident_charP, (word_t) eq3, isn_lookup, isn_call,
                   isn_v_stdout, isn_write_int,
                   isn_lcharP, (word_t) "\n", isn_v_stdout, isn_write_charP,
                   isn_rtn, isn_END, };
    // ++ Y->trace;
    stacky_call(Y, e);
    // -- Y->trace;
  }

  return 0;
}
