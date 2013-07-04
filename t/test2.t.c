#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stky *Y = stky_new();
  {
    char eq1[] = "&eq", eq2[] = "&eq", eq3[] = "&eq";
    stky_i e[] = {
      isn_hdr,
      isn_lit_charP, (stky_i) "@&eq @&eq &eq = ", isn_v_stdout, isn_write_string,
      isn_sym_charP, (stky_i) eq1,
      isn_sym_charP, (stky_i) eq2,
      isn_sym_charP, (stky_i) eq3, isn_lookup, isn_call,
      isn_v_stdout, isn_write_int,
      isn_lit_charP, (stky_i) "\n", isn_v_stdout, isn_write_string,
      isn_rtn, isn_END };
    stky_call(Y, e);
    stky_call(Y, e);
  }
  {
    stky_i sym[] = {
      isn_hdr,
      isn_lit_charP, (stky_i) " sym: ", isn_v_stdout, isn_write_string,
      isn_sym_charP, (stky_i) "foo",    isn_v_stdout, isn_write_symbol,
      isn_lit_charP, (stky_i) "\n",     isn_v_stdout, isn_write_string,
      isn_rtn, isn_END };
    stky_i e[] = {
      isn_hdr,
      isn_lit_voidP, (stky_i) sym, isn_call,
      isn_lit_voidP, (stky_i) sym, isn_call,
      isn_rtn, isn_END };
    stky_call(Y, e);
    stky_call(Y, e);
  }

  return 0;
}
