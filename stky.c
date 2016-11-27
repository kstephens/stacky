#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#include <dlfcn.h>
#include "gc/gc.h"

#define paste2_(A,B) A##B
#define paste2(A,B) paste2_(A,B)

#define s_inline static __inline__ __attribute__((always_inline))

struct s_object;
typedef struct s_object s_object;

typedef void *s_v;
typedef intptr_t s_i;
typedef void (*s_f)();
typedef void *s_o;

#ifndef TYPE
#define TYPE(N) s_v t_##N; struct s_##N; typedef struct s_##N s_##N;
#include "types.h"
#endif

typedef struct s_hdr {
  s_i flags;
  s_v meta;
  s_v type;
} s_hdr;

struct s_object { };
struct s_fun { };
struct s_ref { };
struct s_fixnum { };
struct s_null { };
struct s_mark { };

struct s_type {
  s_v supertype;
  s_v size;
  s_v name;
  s_v id;
  s_v eval;
};

struct s_array {
  size_t s;
  s_v *b, *p, *e;
};

struct s_string {
  size_t s;
  char *b, *p, *e;
};

struct s_u8array {
  size_t s;
  uint8_t *b, *p, *e;
};

struct s_dict {
  s_array _;
  s_v cmp; // a b cmp | {-1,0,1}
};

struct s_symbol {
  s_v name;
};

struct s_cell {
  s_v value;
};

struct s_io {
  s_v opaque;
  s_v name, mode;
};

s_inline int       s_v_t_(s_v v)   { return ((s_i) (v)) & 3; }
s_inline s_v    s_v_f (s_f v)   { return  (s_v) v; }
s_inline s_f    s_v_f_(s_v v)   { return  (s_f) v; }
s_inline s_v    s_v_i (s_i v)   { return  (s_v) (((v) << 2) + 1) ; }
s_inline s_i    s_v_i_(s_v v)   { return ((s_i)  (v)) >> 2       ; }
s_inline s_v    s_v_r (s_v* v)  { return           (((void*) v) + 2); }
s_inline s_v*   s_v_r_(s_v v)   { return (s_v*) (((void*) v) - 2); }
s_inline s_v    s_v_o (s_o v)   { return ((void*) v) + 3; }
s_inline s_o    s_v_o_(s_v v)   { return ((void*) v) - 3; }
s_inline s_hdr* s_v_h_(s_v v)   { return ((s_hdr*) s_v_o_(v)) - 1; }

s_inline s_v    s_v_c (int v)      { return s_v_i(v); }
s_inline int       s_v_c_(s_v v)   { return s_v_i_(v); }
#define s_O(v,type) ((paste2(s_,type)*) s_v_o_(v))

s_v s_v_T_(s_v v)
{
  switch ( s_v_t_(v) ) {
  case 0:   return v ? t_fun : t_null;
  case 1:   return t_fixnum;
  case 2:   return t_ref;
  default:  return s_v_h_(v)->type;
  }
}

void* s_type_alloc(s_v t)
{
  s_type* self = s_O(t, type);
  size_t s = s_v_i_(self->size);
  s_hdr* hdr = GC_malloc(sizeof(*hdr) + s);
  // fprintf(stderr, "  type_alloc %p %lu = @%p\n", t, (unsigned long) s, hdr + 1);
  hdr->flags = (s_i) s_v_i(0);
  hdr->meta = 0;
  hdr->type = t;
  return hdr + 1;
}

static s_array* v_stack;
void s_array_push(s_array *self, s_v v);
s_inline void   s_push(s_v v)    { s_array_push(v_stack, v); }
s_inline s_v s_pop()             { return v_stack->p > v_stack->b ? *(-- v_stack->p) : 0; }
s_inline void   s_popn(size_t n)    { v_stack->p -= n; }
s_inline s_v s_top()             { return v_stack->p[-1]; }

static s_v v_mark;

#define V(i) (v_stack->p[-(i) - 1])

#define s_f(name) paste2(s_f_,name)
#define s_FD(name) void paste2(s_F_,name)(); extern s_f s_f(name);
#ifndef s_F
#define s_F(name) void paste2(s_F_,name)(); s_f s_f(name) = paste2(s_F_,name); void paste2(s_F_,name)()
#endif
#define s_e(name) s_eval(s_f(name))

void s_eval(s_v v);

#define v_END ((s_v) ~0UL)
s_v s_callv(const s_v *v)
{
  while ( v[1] != v_END )
    s_push(*(v ++));
  return *v; // for TAILCALL
}
#define s_call(...)  ({ s_v _argv[] = { __VA_ARGS__, v_END, v_END }; s_eval(s_callv(_argv)); })
#define s_callp(...) ({ s_call(__VA_ARGS__); s_pop(); })

s_v s_execv(const s_v *v)
{
  while ( v[1] != v_END )
    s_eval(*(v ++));
  return *v; // for TAILCALL
}
#define s_exec(...) ({ s_v _argv[] = { __VA_ARGS__, v_END, v_END }; s_eval(s_execv(_argv)); })

#define BOP(n,op) \
  s_F(fx##n)  \
  { \
  V(1) = s_v_i(s_v_i_(V(1)) op s_v_i_(V(0))); \
  s_pop(); \
  }
#define UOP(n,op) \
  s_F(fx##n)  \
  { \
  V(0) = s_v_i(op s_v_i_(V(0))); \
  }
#include "cops.h"

s_FD(print_object);
s_FD(println);

s_F(type) // v | type(v)
{
  V(0) = s_v_T_(V(0));
}
s_F(set_type) // v type | v
{
  s_v type = s_pop();
  s_v v = V(0);
  switch ( s_v_t_(v) ) {
  case 0:   abort();  break;
  case 1:   abort();  break;
  default:  s_v_h_(v)->type = type; break;
  }
}
s_F(nop)  { }
s_F(exec) { s_eval(s_pop()); }
s_F(dup)  { s_push(V(0)); }
s_F(pop)  { s_pop(); }
s_F(popn) { s_popn(s_v_i_(s_pop())); }
s_F(exch) { s_v t = V(0); V(0) = V(1); V(1) = t; }
s_F(mark) { s_push(v_mark); }
s_inline int s_v_execQ(s_v v)
{
  return s_v_h_(v)->flags & 4;
}
s_F(set_exec) { // o | o
  s_v_h_(s_top())->flags |= 4;
}
s_F(meta) { // o | m
  V(0) = s_v_h_(s_top())->meta;
}
s_F(set_meta) { // o m | o
  s_v m = s_pop();
  s_v_h_(s_top())->meta = m;
}
s_F(cell_new) { // v | c(v)
  s_cell* self = s_type_alloc(t_cell);
  self->value = V(0);
  V(0) = s_v_o(self);
}
s_F(eval_cell) { // cell | value(cell)
  V(0) = s_O(V(0), cell)->value;
}
s_F(ref_make) { // * | r(*)
  V(0) = s_v_r(V(0));
}
s_F(ref_new) { // v | r(v)
  s_e(cell_new);
  V(0) = s_v_r(s_v_o_(V(0)));
}
s_F(ref_get) { // r | *r
  V(0) = *s_v_r_(V(0));
}
s_F(ref_set) { // v r |
  *s_v_r_(V(0)) = V(1);
  s_popn(2);
}
s_F(eval_ref) {  // ref | *ref
  V(0) = *s_v_r_(V(0));
}

s_F(v_stack) { s_push(s_v_o(v_stack)); }
s_F(count_to_mark) // [ ... mark v1 v2 .. vn ] | n
{
  size_t c = 0;
  s_array *a = s_v_o_(s_pop());
  for ( s_v* v = a->p; -- v > a->b && *v != v_mark; ++ c ) ;
  s_push(s_v_i(c));
}

#define T array
#define T_t s_v
#define T_box(V) (V)
#define T_unbox(V) (V)
#include "vec.c"

#define T string
#define T_t char
#define T_box(V)   s_v_c(V)
#define T_unbox(V) s_v_c_(V)
#include "vec.c"

#define T u8array
#define T_t uint8_t
#define T_box(V)   s_v_i(V)
#define T_unbox(V) s_v_i_(V)
#include "vec.c"


s_v s_string_new(const char *v, int s)
{
  if ( s < 0 ) s = strlen(v);
  s_string* self = s_type_alloc(t_string);
  s_string_init(self, s);
  memcpy(self->b, v, sizeof(self->b[0]) * (s + 1));
  self->p = self->b + s;
  return s_v_o(self);
}

s_v array_exec_begin, array_exec_end;
int defer_eval = 0;
s_F(eval_inner_deferred)
{
  if ( ! defer_eval ) {
    if ( s_v_T_(s_top()) == t_symbol )
      s_e(exec);
    s_e(exec);
  }
}

s_F(eval_inner)
{
  s_v token = s_top();
  if ( token == array_exec_end ) {
    defer_eval --;
    s_e(eval_inner_deferred);
  } else if ( token == array_exec_begin ) {
    s_e(eval_inner_deferred);
    defer_eval ++;
  } else {
    s_e(eval_inner_deferred);
  }
}

static s_array* e_stack;
s_inline void   s_e_push(s_v v)  { s_array_push(e_stack, v); }
s_inline s_v s_e_pop()           { return *(-- e_stack->p); }
// s_inline s_v s_e_top()           { return e_stack->p[-1]; }
s_F(e_stack) { s_push(s_v_o(e_stack)); }
s_F(eval_array) {
  if ( s_v_execQ(V(0)) ) {
    s_array* a = s_O(s_pop(), array);
    s_v *p = a->b;
    if ( p < a->p ) {
      s_e_push(s_v_o(a));
      while ( p + 1 < a->p ) {
        s_push(*(p ++));
        s_e(eval_inner);
      }
      s_e_pop();
      s_push(*p);
      s_e(eval_inner);
    }
  }
}

s_F(cmp_word) {
  s_v b = s_pop();
  s_v a = V(0);
  V(0) = s_v_i(a < b ? -1 : a == b ? 0 : 1);
}
s_F(cmp_string) {
  s_string* b = s_O(s_pop(), string);
  s_string* a = s_O(s_pop(), string);
  size_t al = a->p - a->b;
  size_t bl = b->p - b->b;
  int c = memcmp(a->b, b->b, al < bl ? al : bl);
  if ( c == 0 ) c = al < bl ? -1 : al == bl ? 0 : 1;
  s_push(s_v_i(c));
}
s_F(array_to_dict) // [ k1 v1 ... kn vn ] | @[ dict ]@
{
  s_array* a = s_v_o_(s_pop());
  s_dict* d = s_type_alloc(t_dict);
  size_t s = a->p - a->b;
  s_array_init(&d->_, s);
  memcpy(d->_.b, a->b, sizeof(a->p[0]) * s);
  d->_.p += s;
  for ( s_v *p = d->_.b; p < d->_.p ; p += 2 ) {
    p[1] = s_callp(p[1], s_f(ref_new));
  }
  d->cmp = s_f(cmp_word);
  s_push(s_v_o(d));
}
s_F(dict_get_ref) {  // k dict | r
  s_dict* d = s_O(s_pop(), dict);
  s_v v = 0;
  s_v k = s_pop();
  s_v *p = d->_.b;
  while ( p < d->_.p ) {
    if ( ! s_v_i_(s_callp(k, p[0], d->cmp)) )
      { v = p[1]; break; }
    p += 2;
  }
  s_push(v);
}
s_F(dict_set_ref) { // k r dict | 
  s_dict* d = s_O(s_pop(), dict);
  s_v v = s_pop();
  s_v k = s_pop();
  s_v *p = d->_.b;
  while ( p < d->_.p ) {
    if ( ! s_v_i_(s_callp(k, p[0], d->cmp)) )
      { p[1] = v; return; }
    p += 2;
  }
  s_call(k, s_v_o(d), s_f(array_push));
  s_call(v, s_v_o(d), s_f(array_push));
}
s_F(dict_add) { // k v dict |
  s_v d = s_pop();
  s_v v = s_pop();
  s_v k = s_pop();
  s_call(k, d, s_f(array_push));
  s_call(v, s_f(ref_new));
  s_call(d, s_f(array_push));
}
s_F(dict_get) { // k dict | v
  s_e(dict_get_ref);
  if ( V(0) ) V(0) = *s_v_r_(V(0));
}
s_F(dict_set) { // k v dict |
  s_v d = s_pop();
  s_v v = s_pop();
  s_v k = s_pop();
  s_call(k, d, s_f(dict_get_ref));
  if ( V(0) ) {
    *s_v_r_(V(0)) = v;
  } else {
    s_pop();
    s_call(k, v, d, s_f(dict_add));
  }
}
s_F(dict_getsert) { // k v dict | v
  s_v d = s_pop();
  s_v v = s_pop();
  s_v k = s_pop();
  s_call(k, d, s_f(dict_get_ref));
  if ( V(0) ) {
    V(0) = *s_v_r_(V(0));
  } else {
    V(0) = v;
    s_call(k, v, d, s_f(dict_add));
  }
}
s_F(dicts_get) { // k dicts | v
  s_array* ds = s_O(s_pop(), array);
  s_v k = s_pop();
  s_v* p = ds->p;
  while ( p > ds->b ) {
    s_call(k, *(-- p), s_f(dict_get_ref));
    if ( s_top() ) {
      s_e(ref_get);
      return;
    }
    s_pop();
  }
  s_push(0);
}
s_F(eval_dict) {
  if ( s_v_execQ(V(0)) ) {
    s_e(dict_get);
  }
}

static s_array* d_stack;
s_F(d_stack) { s_push(s_v_o(d_stack)); }
s_F(eval_symbol) { // sym | v
  s_e(d_stack);
  s_e(dicts_get);
}

static s_v sym_dict;
s_F(sym_dict) { s_push(sym_dict); }
s_F(string_to_symbol) { // str | sym
  s_v sym = s_v_o(s_type_alloc(t_symbol));
  s_O(sym, symbol)->name = V(0);
  s_call(sym, sym_dict, s_f(dict_getsert));
}
s_F(charP_to_string) { // char* | string
  s_push(s_string_new(s_pop(), -1));
}
s_F(string_to_fun) { // str | fun
  s_string* str = s_O(s_pop(), string);
  void *fun = dlsym(RTLD_DEFAULT, str->b);
  s_push(s_v_f(fun));
}
s_F(fun_to_string) { // fun | str
  struct dl_info info = { 0 };
  void *fun = s_v_f_(s_pop());
  if ( dladdr(fun, &info) )
    s_push(s_string_new(info.dli_sname, -1));
  else
    s_push(0);
}

s_F(io_write_string) // str IO |
{
  s_io* io = s_O(s_pop(), io);
  s_string* str = s_O(s_pop(), string);
  fwrite(str->b, sizeof(str->b[0]), str->p - str->b, io->opaque);
}
s_F(read_char) { // IO | c
  s_io* io = s_O(V(0), io);
  V(0) = s_v_c(fgetc(io->opaque));
}
s_F(unread_char) { // c IO |
  s_io* io = s_O(s_pop(), io);
  s_v c = s_pop();
  ungetc(s_v_c_(c), io->opaque);
}
s_F(at_eos) { // IO | eos?
  s_io* io = s_O(V(0), io);
  V(0) = s_v_i(feof(io->opaque));
}
s_F(close) { // IO |
  s_io* io = s_O(s_pop(), io);
  if ( io->opaque )
    fclose(io->opaque);
  io->opaque = 0;
}

s_v s_io_new_FILEP(FILE *fp, const char *name, const char *mode)
{
  s_io *o = s_type_alloc(t_io);
  if ( ! fp && ! (fp = fopen(name, mode)) )
    perror("Cannot open file");
  o->opaque  = fp;
  o->name    = s_string_new(name, -1);
  o->mode    = s_string_new(mode, -1);
  return s_v_o(o);
}
s_v s_stdin, s_stdout, s_stderr;

enum read_state {
    rs_error = -2,
    rs_eos = -1,
    rs_stop = 0,
    rs_start,
    rs_comment_eol,
    rs_symbol,
    rs_literal,
    rs_pot_num,
    rs_int,
    rs_char,
    rs_string,
};

s_F(read_token)
{
  s_io *in = s_O(s_pop(), io);
  s_v value = 0;
  s_v token = 0;
  s_i n = 0;
  int pot_num_c = 0;
  int literal = 0;
  int state = rs_start, last_state = -1, last_state_2 = rs_start;
  int c = -2;
   
#define take_c() c = -2
#define next_c() take_c(); goto again
#define next_s(s) state = (s); goto again
#define stop_s(s) last_state_2 = (s); next_s(rs_stop)
 again:
  last_state = last_state_2; last_state_2 = state;
  if ( ! token ) token = s_string_new("", 0);
  if ( c == -2 ) {
    c = s_v_c_(s_callp(s_v_o(in), s_f(read_char)));
  }
  switch ( state ) {
  case rs_error:
    s_call(s_v_c(c), token, s_f(string_push));
    value = token;
  case rs_eos:
  case rs_stop:
    break;
  case rs_start:
    switch ( c ) {
    case EOF: stop_s(rs_eos);
    case ' ': case '\t': case '\n': case '\r':
      next_c();
    case '%': take_c(); next_s(rs_comment_eol);
    case '/':
      literal ++; next_c();
    case '-': case '+':
      s_call(s_v_c(c), token, s_f(string_push));
      pot_num_c = c; take_c();
      next_s(rs_pot_num);
    case '0' ... '9':
      next_s(rs_int);
    case '\'':
      state = rs_char; next_c();
    case '"':
      state = rs_string; next_c();
    default:
      next_s(rs_symbol);
    }
  case rs_comment_eol:
    switch ( c ) {
    case '\n':
      take_c(); next_s(rs_start);
    case EOF:
      next_s(rs_start);
    default:
      next_c();
    }
  case rs_symbol:
    switch ( c ) {
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      value = s_callp(token, s_f(string_to_symbol));
      next_s(rs_stop);
    default:
      s_call(s_v_c(c), token, s_f(string_push));
      next_c();
    }
  case rs_pot_num:
    switch ( c ) {
    case '0' ... '9':
      next_s(rs_int);
    default:
      next_s(rs_symbol);
    }
  case rs_int:
    switch ( c ) {
    case '0' ... '9':
      s_call(s_v_c(c), token, s_f(string_push));
      {
        s_i prev_n = n;
        n = n * 10 + c - '0';
        if ( n < prev_n ) { take_c(); next_s(rs_symbol); } // overflow
        // fprintf(stderr, "  s_int: n = %lld\n", (long long) n);
      }
      next_c();
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      if ( pot_num_c == '-' ) n = - n;
      value = s_v_i(n); next_s(rs_stop);
    default:
      next_s(rs_symbol);
    }
  case rs_char:
    switch ( c ) {
    default:
      value = s_v_c(c); next_s(rs_stop);
    }
  case rs_string:
    switch ( c ) {
    case '"':
      value = token; take_c(); next_s(rs_stop);
    default:
      s_call(s_v_c(c), token, s_f(string_push));
      next_c();
    }
  default:
    abort();
  }
#undef take_c
#undef next_c
#undef next_s
#undef stop_s
  // stop:
  if ( c >= 0 )
    s_call(s_v_c(c), s_v_o(in), s_f(unread_char));
  while ( literal -- ) {
    value = s_callp(value, s_f(cell_new));
    last_state = rs_literal;
  }
  s_push(value);
  s_push(s_v_i(last_state));
}

void s_eval(s_v v)
{
 again:
  switch ( s_v_t_(v) ) {
  case 0:
    if ( v ) (s_v_f_(v))();  // proc: invoke it.
    else     goto other;        // null: other ...
    break;
  case 1:  s_push(v);      break; // integer: push it.
  case 2:  s_push(v); s_e(eval_symbol);  break; // symbol: lookup->exec or push it. 
  default: other: // other: other...
    s_push(v);
    v = s_O(s_v_T_(v), type)->eval;
    goto again;
  }
}

s_F(if_else) { // t f v | (t eval or f eval)
  if ( ! V(0) ) V(2) = V(1);
  s_popn(2);
  s_e(exec);
}
s_F(or) { // a b | (a or b)
  if ( ! V(1) ) V(1) = V(0);
  s_popn(1);
}
s_F(and) { // a b | (a and b)
  if ( V(1) ) V(1) = V(0);
  s_popn(1);
}

s_F(write_charP) {
  printf("%s", s_pop());
}
s_F(write_string) {
  s_string* s = s_O(s_pop(), string);
  fwrite(s->b, sizeof(s->b[0]), s->p - s->b, stdout);
}
s_F(print_voidP) {
  printf("%p", s_pop());
}

s_F(array_end) { // mark v0 .. vn | [ v0 .. vn ]
  s_exec(s_f(v_stack), s_f(count_to_mark),
            s_f(array_make), s_f(exch), s_f(pop));
}  
s_F(array_end_exec) { // mark v0 .. vn | { v0 .. vn }
  s_exec(s_f(array_end), s_f(set_exec)); 
}
s_F(make_selector) { // default_method
  s_v default_method = s_pop();
  s_call(v_mark, s_f(array_end));
  s_v methods = s_callp(s_f(array_to_dict));
  s_call((s_v) 0, default_method, methods, s_f(dict_set));
  s_call(v_mark,
            s_f(dup),   // obj | obj
            s_f(type),  // obj | type(obj)
            methods,       // obj | type(obj) methods
            s_f(dict_get), // obj | method(type)
            0,
            methods,
            s_f(dict_get), // obj | method(type) default_method
            s_f(or),  // obj | method
            s_f(exec), // obj | ...
            s_f(array_end_exec));
  s_call(methods, s_f(set_meta));
}
s_F(add_method) { // type method selector
  s_e(meta);
  s_e(dict_set);
}

s_v print_methods;
s_FD(print_object);
s_F(print) {
  s_v v = V(0);
  if ( s_v_o_(v) == v_stack ) {
    s_pop();
    printf("<<v_stack>>");
    return;
  }
  s_push(v);
  s_e(type);
  s_push(print_methods);
  s_e(dict_get);
  s_push(s_f(print_object));
  s_e(or);
  s_e(exec);
}
s_F(print_space) {
  s_e(print);
  s_call(" ", s_f(write_charP));
}
s_F(print_fun) {
  s_call("@fun:", s_f(write_charP));
  s_e(dup);
  s_e(fun_to_string);
  if ( s_top() ) {
    s_e(write_string);
    s_pop();
  } else {
    s_pop();
    s_e(print_voidP);
  }
}
s_F(print_fixnum) {
  printf("%lld", (long long) s_v_i_(s_pop()));
}
s_F(print_symbol) {
  s_call(s_O(s_pop(), symbol)->name, s_f(write_string));
}
s_F(print_string) {
  s_call("\"", s_f(write_charP));
  s_call(s_f(write_string));
  s_call("\"", s_f(write_charP));
}
s_F(print_cell) {
  s_call("/", s_f(write_charP));
  s_call(s_O(s_pop(), cell)->value, s_f(print));
}
s_F(print_ref) {
  s_call("@", s_f(write_charP));
  s_e(print_voidP);
}
s_F(print_type) {
  s_call("@type:", s_f(write_charP));
  s_call(s_O(s_pop(), type)->name, s_f(write_string));
}
s_F(print_array) {
  int exec = s_v_execQ(s_top());
  printf(exec ? "{ " : "[ ");
  s_call(s_f(print_space), s_f(array_each));
  printf(exec ? "}" : "]");
}
s_F(print_dict) {
  printf("<< ");
  s_call(s_f(print_space), s_f(array_each));
  printf(">>");
}
s_F(print_mark) {
  s_pop();
  printf("<<mark>>");
}
s_F(print_object) {
  s_e(dup);
  s_e(type);
  s_e(print);
  printf(":%p", s_pop());
}
s_F(println) {
  s_e(print);
  s_call("\n", s_f(write_charP));
}

s_F(eval_io) {
  s_v io = s_pop();
  int echo_token = 0;
  int print_result = 0;
  
  while ( s_call(io, s_f(at_eos)), ! s_v_i_(s_pop()) ) {
    s_call(io, s_f(read_token));
    s_v state = s_pop();
    if ( echo_token ) {
      s_call(state, s_f(println));
      s_e(dup);
      s_e(println);
    }
    if ( s_v_i_(state) < 0 ) {
      s_pop();
      break;
    }
    s_e(eval_inner);
    if ( print_result ) {
      s_e(dup);
      s_e(println);
    }
    // s_call(sym_dict, s_f(println));
    printf("  # v_stack: ");
    s_call(s_v_o(v_stack), s_f(print_array));
    printf("\n");
  }
}

s_v s_symbol_new(const char* s)
{
  return s_callp(s_string_new(s, -1), s_f(string_to_symbol));
}

s_v core_dict;
s_F(core_dict) { s_push(core_dict); }

void s_def_ref(const char *k, void *r)
{
  s_call(s_symbol_new(k), r, s_f(ref_make));
  s_call(core_dict, s_f(dict_set_ref));
}

void s_init()
{
  int id = 0;
  GC_init();
  t_type = s_v_o(GC_malloc(sizeof(s_hdr) + sizeof(s_type)) + sizeof(s_hdr));
  s_O(t_type, type)->size = s_v_i(sizeof(s_type));
#ifndef TYPE
#define TYPE(N)                                                 \
  t_##N = t_##N ?: s_v_o(s_type_alloc(t_type));            \
  s_O(t_type, type)->size = s_v_i(sizeof(s_type));
  TYPE(type)
#include "types.h"

#define TYPE(N)                                        \
  s_v_h_(t_##N)->type = t_type;                     \
  s_O(t_##N, type)->name = s_string_new(#N, -1);   \
  s_O(t_##N, type)->size = s_v_i(sizeof(s_##N)); \
  s_O(t_##N, type)->id   = s_v_i(++ id); \
  s_O(t_##N, type)->eval = s_f(nop);
#include "types.h"
#endif

  s_O(t_array , type)->eval = s_f(eval_array);
  s_O(t_dict  , type)->eval = s_f(eval_dict);
  s_O(t_symbol, type)->eval = s_f(eval_symbol);
  s_O(t_io,     type)->eval = s_f(eval_io);
  s_O(t_cell,   type)->eval = s_f(eval_cell);

  v_mark = s_v_o(s_type_alloc(t_mark));
  s_array_init(v_stack = s_type_alloc(t_array), 1024);
  s_array_init(e_stack = s_type_alloc(t_array), 1024);
  s_array_init(d_stack = s_type_alloc(t_array), 1024);

  s_push(s_v_i(0));
  s_exec(s_f(array_make), s_f(array_to_dict));
  sym_dict = s_pop();
  s_O(sym_dict, dict)->cmp = s_f(cmp_string);

  s_call(v_mark,
            t_fun,    s_f(print_fun),
            t_fixnum, s_f(print_fixnum),
            t_symbol, s_f(print_symbol),
            t_string, s_f(print_string),
            t_type,   s_f(print_type),
            t_array,  s_f(print_array),
            t_dict,   s_f(print_dict),
            t_cell,   s_f(print_cell),
            t_ref,    s_f(print_ref),
            t_mark,   s_f(print_mark),
            s_f(nop));
  s_exec(s_f(v_stack), s_f(count_to_mark),
            s_f(array_make), s_f(array_to_dict),
            s_f(exch), s_f(pop));
  print_methods = s_pop();
  // s_call(print_methods, s_f(println));
  s_call(s_f(print_object), s_f(make_selector));
  s_call(s_f(println));

  s_stdin  = s_io_new_FILEP(stdin,  "<stdin>",  "r");
  s_stdout = s_io_new_FILEP(stdout, "<stdout>", "w");
  s_stderr = s_io_new_FILEP(stderr, "<stderr>", "w");

  s_push(s_v_i(0));
  s_exec(s_f(array_make), s_f(array_to_dict));
  core_dict = s_pop();
  s_call(core_dict, s_v_o(d_stack), s_f(array_push));
  array_exec_begin = s_symbol_new("{");
  array_exec_end = s_symbol_new("}");
#ifndef TYPE
#define TYPE(N) s_def_ref("&<" #N ">", &t_##N);
#include "types.h"
#endif
#define F(N) s_def_ref("&" #N, &s_f(N))
#define def_s_F(N) F(N);
#include "prims.h"

  // NON-CORE:
  F(print);
  F(println);
#undef F
  s_v io = s_io_new_FILEP(0, "boot.stky", "r");
  s_call(io, s_f(eval_io));
  s_call(io, s_f(close));
}

int main(int argc, char **argv, char **envp)
{
  s_init();
  s_call(core_dict, s_f(println));
  s_call(s_stdin, s_f(eval_io));
  return 0;
}
