#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stacky *Y = stacky_new();
  {
    char eq1[] = "%eq", eq2[] = "%eq", eq3[] = "%eq";
    stky_i e[] = { isn_hdr,
                   isn_lit_charP, (stky_i) "@%eq @%eq %eq = ", isn_v_stdout, isn_write_string,
                   isn_ident_charP, (stky_i) eq1,
                   isn_ident_charP, (stky_i) eq2,
                   isn_ident_charP, (stky_i) eq3, isn_lookup, isn_call,
                   isn_v_stdout, isn_write_int,
                   isn_lit_charP, (stky_i) "\n", isn_v_stdout, isn_write_string,
                   isn_rtn, isn_END, };
    stacky_call(Y, e);
    stacky_call(Y, e);
  }
  {
    stky_i ident[] = {
      isn_hdr,
      isn_lit_charP, (stky_i) " ident: ", isn_v_stdout, isn_write_string,
      isn_ident_charP, (stky_i) "foo",    isn_v_stdout, isn_write_symbol, 
      isn_lit_charP, (stky_i) "\n",       isn_v_stdout, isn_write_string,
      isn_rtn, isn_END,
    };
    stky_i e[] = { isn_hdr,
                   isn_lit_voidP, (stky_i) ident, isn_call,
                   isn_lit_voidP, (stky_i) ident, isn_call,
                   isn_rtn, isn_END, };
    stacky_call(Y, e);
    stacky_call(Y, e);
  }

  return 0;
}
