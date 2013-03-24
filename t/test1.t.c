#include "stacky/stacky.h"

int main(int argc, char **argv)
{
  stacky *Y = stacky_new();
  {
  word_t t[] = {
    isn_hdr,
    isn_lcharP, (word_t) " true\n", isn_v_stdout, isn_write_charP,
    isn_rtn,
    isn_END,
  };
  word_t f[] = {
    isn_hdr,
    isn_lcharP, (word_t) " false\n", isn_v_stdout, isn_write_charP,
    isn_rtn,
    isn_END,
  };
  word_t eq[] = {
    isn_hdr,
    isn_eq,
    isn_rtn,
    isn_END,
  };
  word_t ident[] = {
    isn_hdr,
    isn_lcharP, (word_t) " ident: ", isn_v_stdout, isn_write_charP,
    isn_ident_charP, (word_t) "foo", isn_v_stdout, isn_write_charP, 
    isn_lcharP, (word_t) "\n",       isn_v_stdout, isn_write_charP,
    isn_pop,
    isn_rtn, isn_END,
  };
  word_t isns[] = {
    isn_hdr,
    isn_lint, 2,
    isn_lint, 3,
    isn_add,
    isn_lint, 5,
    isn_mul,
    isn_dup,
    isn_v_stdout, isn_write_int,
    isn_lint, 25,
    isn_ge,
    isn_ifelser, (word_t) 0, (word_t) 5,
    isn_lvoidP, (word_t) t, isn_call,
    isn_jmpr, 3,
    isn_lvoidP, (word_t) f, isn_call,
    isn_lvoidP, (word_t) eq, isn_dict_new,
    isn_lint, 1, isn_lint, 2, isn_dict_set,
    isn_dup, isn_lint, 1, isn_lint, 0, isn_dict_get,
    isn_v_stdout, isn_write_int, isn_lcharP, (word_t) "\n", isn_v_stdout, isn_write_charP,
    isn_dup, isn_lint, 3, isn_lint, 0, isn_dict_get,
    isn_v_stdout, isn_write_int, isn_lcharP, (word_t) "\n", isn_v_stdout, isn_write_charP,
    isn_pop,
    isn_lvoidP, (word_t) ident, isn_call,
    isn_lvoidP, (word_t) ident, isn_call,
    isn_rtn,
    isn_END,
  };
  stacky_call(Y, isns);
  }

  return 0;
}
