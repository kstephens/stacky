#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stacky *Y = stacky_new();
  {
  stky_i t[] = {
    isn_hdr,
    isn_lit_charP, (stky_i) " true\n", isn_v_stdout, isn_write_string,
    isn_rtn, isn_END };
  stky_i f[] = {
    isn_hdr,
    isn_lit_charP, (stky_i) " false\n", isn_v_stdout, isn_write_string,
    isn_rtn, isn_END };
  stky_i eq[] = { isn_hdr, isn_eq, isn_rtn, isn_END, };
  stky_i isns[] = {
    isn_hdr,
    isn_lit_int, 2,
    isn_lit_int, 3,
    isn_add,
    isn_lit_int, 5,
    isn_mul,
    isn_dup,
    isn_v_stdout, isn_write_int,
    isn_lit_int, 25,
    isn_ge,
    isn_ifelser, (stky_i) 0, (stky_i) 5,
    isn_lit_voidP, (stky_i) t, isn_call,
    isn_jmpr, 3,
    isn_lit_voidP, (stky_i) f, isn_call,
    isn_lit_voidP, (stky_i) eq, isn_dict_new,
    isn_lit_int, 1, isn_lit_int, 2, isn_dict_set,
    isn_dup, isn_lit_int, 1, isn_lit_int, 0, isn_dict_get,
    isn_v_stdout, isn_write_int, isn_lit_charP, (stky_i) "\n", isn_v_stdout, isn_write_string,
    isn_dup, isn_lit_int, 3, isn_lit_int, 0, isn_dict_get,
    isn_v_stdout, isn_write_int, isn_lit_charP, (stky_i) "\n", isn_v_stdout, isn_write_string,
    isn_pop,
    isn_rtn, isn_END };
  stacky_call(Y, isns);
  }

  return 0;
}
