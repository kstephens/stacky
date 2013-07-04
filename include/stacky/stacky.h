#ifndef _stacky_stacky_h
#define _stacky_stacky_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

typedef void *voidP;
typedef char *charP;
typedef unsigned char *ucharP;

typedef void *stky_v;
typedef size_t  stky_w;
typedef ssize_t stky_i;

typedef struct stky_object {
  struct stky_type *type;
  stky_i flags;
  stky_i size;
} stky_object;

typedef struct stky_type {
  stky_object o;
  stky_i i;         // stky_t_*; index in Y->types[].
  const char *name;
} stky_type;

typedef struct stky_isn {
  stky_object o;
  int i;
  stky_i isn;
  int nwords;
  const char *name;
  const char *sym_name;
  void *addr;
  struct stky_words *words;
} stky_isn;

typedef struct stky_bytes {
  stky_object o;
  void *b, *p;
  stky_i l, s, es;
} stky_bytes;
typedef stky_bytes *stky_bytesP;

typedef struct stky_array {
  stky_object o;
  stky_v *b, *p;
  stky_i l, s, es;
} stky_array;
typedef stky_array *stky_arrayP;

typedef struct stky_string {
  stky_object o;
  char *b, *p;
  stky_i l, s, es;
} stky_string;
typedef stky_string *stky_stringP;

typedef struct stky_words {
  stky_object o;
  stky_i *b, *p;
  stky_i l, s, es;
  const char *name;
} stky_words;
typedef stky_words *stky_wordsP;

typedef struct stky_symbol {
  stky_object o;
  stky_string *name;
} stky_symbol;
typedef stky_symbol *stky_symbolP;

typedef struct stky_dict {
  stky_array a;
  stky_v eq;
} stky_dict;
typedef stky_dict *stky_dictP;

typedef struct stky_literal {
  stky_object o;
  stky_v value;
} stky_literal;
typedef stky_literal *stky_literalP;

typedef struct stky_io {
  stky_object o;
  FILE *fp;
  stky_v name, mode;
} stky_io;

typedef struct stky_catch {
  stky_object o;
  jmp_buf jb;
  stky_v thrown, value;
  stky_v vs_depth, es_depth;
  struct stky_catch *prev, *prev_error_catch;
  stky_i defer_eval;
} stky_catch;
typedef stky_catch *stky_catchP;


#define stky_v_bits 2
#define stky_v_mask 3
#define stky_v_tag(V)  (((stky_i) (V)) & 3)
#define stky_v_type(V)                                         \
  ( ! (V) ? Y->types + stky_t_null :                           \
    ! stky_v_tag(V) ? *(stky_type**)(V) :                     \
    Y->types + stky_v_tag(V) )
#define stky_v_type_i(V) stky_v_type(V)->i
#define _stky_v_int(X)                       (((X)  << 2) | 1)
#define stky_v_int(X)    ((stky_v) ((((stky_i) (X)) << 2) | 1))
#define stky_v_int_(V)              (((stky_i) (V)) >> 2)
#define stky_v_char(X)   ((stky_v) ((((stky_w) (X)) << 2) | 2))
#define stky_v_char_(V)             (((stky_w) (V)) >> 2)
#define stky_v_isnQ(V) (stky_v_tag(V) == stky_t_tag)
#define _stky_v_isn(X)                       (((X)  << 2) | 3)
#define stky_v_isn(X)                ((stky_v) (X))
#define stky_v_isn_(V)               ((stky_w) (V))

#include "isn.h"

enum stky_type_e {
#define TYPE(name) stky_t_##name,
  styk_t_BEGIN = -1,
#include "stacky/types.h"
  stky_t_END
};

typedef struct stky {
  stky_object o;
  stky_array vs, es;
  stky_i trace, token_debug, threaded_comp;
  stky_v v_stdin, v_stdout, v_stderr;
  stky_v v_mark, v_marke, v_lookup_na;
  stky_v s_mark, s_marke, s_array_tme;
  stky_dict *sym_dict;
  stky_array *dict_stack;
  stky_dict *dict_0;
  stky_type types[stky_t_END + 1];
#define stky_t(name) (Y->types + stky_t_##name)
  stky_isn isns[isn_END + 1];
#define stky_isn_w(I) ((stky_i) Y->isns[I].words)
  stky_i defer_eval;
  stky_catch *current_catch, *error_catch;
  stky_v not_found;
} stky;

stky *stky_new();

stky *stky_call(stky *Y, stky_i *pc);
#define stky_words(Y,WORDS...) \
  ({ stky_i _words_##__LINE__[] = { isn_hdr, WORDS, 0 }; stky_words_new((Y), _words_##__LINE__, sizeof(_words_##__LINE__) / sizeof(stky_i)); })
#define stky_exec(Y,WORDS...)                                            \
  ({ stky_i _words_##__LINE__[] = { WORDS, isn_rtn, 0 }; stky_call((Y), _words_##__LINE__); })

stky_v stky_pop(stky *Y);

stky_v stky_read_token(stky *Y, FILE *in);
stky *stky_repl(stky *Y, FILE *in, FILE *out);

stky *stky_write(stky *Y, stky_v v, FILE *out, int depth);

#define stky_catch__BODY(NAME) {                \
  stky_catch *NAME = stky_catch__new(Y);        \
  switch ( sigsetjmp(NAME->jb, 1) ) {           \
  case 0:
#define stky_catch__THROWN(NAME)                          \
  break;                                                  \
  default:
#define stky_catch__END(NAME)                           \
  break;                                                \
  }                                                     \
}
stky_catch *stky__catch__new(stky *Y);
void stky_catch__throw(stky *Y, stky_catch *catch, stky_v value);

#endif
