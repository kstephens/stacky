#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <dlfcn.h>
#include "gc/gc.h"

#define stky_inline static __inline__ __attribute__((always_inline))

struct stky_object;
typedef struct stky_object stky_object;

typedef void *stky_v;
typedef intptr_t stky_i;
typedef void (*stky_f)();
typedef void *stky_o;

#define TYPE(N) stky_v t_##N; struct stky_##N; typedef struct stky_##N stky_##N;
#include "types.h"

typedef struct stky_hdr {
  stky_i flags;
  stky_v meta;
  stky_v type;
} stky_hdr;

struct stky_object { };
struct stky_fun { };
struct stky_fixnum { };
struct stky_null { };
struct stky_mark { };

struct stky_type {
  stky_v supertype;
  stky_v size;
  stky_v name;
  stky_v id;
  stky_v eval;
};

struct stky_array {
  size_t s;
  stky_v *b, *p, *e;
};

struct stky_dict {
  stky_array _;
  stky_v cmp; // a b cmp | {-1,0,1}
};

struct stky_string {
  size_t s;
  char *b, *p, *e;
};

struct stky_symbol {
  stky_v name;
};

struct stky_cell {
  stky_v value;
};

struct stky_io {
  stky_v opaque;
  stky_v name, mode;
  stky_v write_string;
  stky_v read_char, unread_char, at_eos, close;
};

stky_inline int       stky_v_t_(stky_v v)   { return ((stky_i) (v)) & 3; }
stky_inline stky_v    stky_v_f (stky_f v)   { return  (stky_v) v; }
stky_inline stky_f    stky_v_f_(stky_v v)   { return  (stky_f) v; }
stky_inline stky_v    stky_v_i (stky_i v)   { return  (stky_v) (((v) << 2) + 1) ; }
stky_inline stky_i    stky_v_i_(stky_v v)   { return ((stky_i)  (v)) >> 2       ; }
stky_inline stky_v    stky_v_o (stky_o v)   { return ((void*) v) + 3; }
stky_inline stky_o    stky_v_o_(stky_v v)   { return ((void*) v) - 3; }
stky_inline stky_hdr* stky_v_h_(stky_v v)   { return ((stky_hdr*) stky_v_o_(v)) - 1; }

stky_inline stky_v    stky_v_c (int v)      { return stky_v_i(v); }
stky_inline int       stky_v_c_(stky_v v)   { return stky_v_i_(v); }
#define stky_O(v,type) ((stky_##type*) stky_v_o_(v))

stky_v stky_v_T_(stky_v v)
{
  switch ( stky_v_t_(v) ) {
  case 0:   return v ? t_fun : t_null;
  case 1:   return t_fixnum;
  case 2:   return t_symbol;
  default:  return stky_v_h_(v)->type;
  }
}

void* stky_type_alloc(stky_v t)
{
  stky_type* self = stky_O(t, type);
  size_t s = stky_v_i_(self->size);
  stky_hdr* hdr = GC_malloc(sizeof(*hdr) + s);
  hdr->flags = (stky_i) stky_v_i(0);
  hdr->meta = 0;
  hdr->type = t;
  return hdr + 1;
}

void stky_array_resize(stky_array *self, size_t s)
{
  size_t ms = sizeof(self->b[0]) * (s + 1);
  stky_v *b = self->b ?
    GC_realloc(self->b, ms) :
    GC_malloc(ms);
  self->p = b + (self->p - self->b);
  *(self->e = (self->b = b) + (self->s = s)) = 0;
}
void stky_array_init(stky_array* self, size_t s)
{
  self->b = self->p = self->e = 0;
  self->s = 0;
  stky_array_resize(self, s);
}
void stky_array_push(stky_array *self, stky_v v)
{
  if ( self->p >= self->e )
    stky_array_resize(self, self->s * 3 / 2 + 3);
  *(self->p ++) = v;
}
stky_v stky_array_pop(stky_array *self)
{
  if ( self->p <= self->b )
    abort();
  return *(-- self->p);
}

void stky_string_resize(stky_string *self, size_t s)
{
  size_t ms = sizeof(self->b[0]) * (s + 1);
  char *b = self->b ?
    GC_realloc(self->b, ms) :
    GC_malloc_atomic(ms);
  self->p = b + (self->p - self->b);
  *(self->e = (self->b = b) + (self->s = s)) = 0;
}
void stky_string_init(stky_string* self, size_t s)
{
  self->b = self->p = self->e = 0;
  self->s = 0;
  stky_string_resize(self, s);
}
stky_v stky_string_new(const char *v, int s)
{
  if ( s < 0 ) s = strlen(v);
  stky_string* self = stky_type_alloc(t_string);
  stky_string_init(self, s);
  memcpy(self->b, v, sizeof(self->b[0]) * (s + 1));
  self->p = self->b + s;
  return stky_v_o(self);
}

static stky_array* v_stack;
stky_inline void   stky_push(stky_v v)    { stky_array_push(v_stack, v); }
stky_inline stky_v stky_pop()             { return v_stack->p > v_stack->b ? *(-- v_stack->p) : 0; }
stky_inline void   stky_popn(size_t n)    { v_stack->p -= n; }
stky_inline stky_v stky_top()             { return v_stack->p[-1]; }

static stky_array* e_stack;
stky_inline void   stky_e_push(stky_v v)  { stky_array_push(e_stack, v); }
stky_inline stky_v stky_e_pop()           { return *(-- e_stack->p); }
stky_inline stky_v stky_e_top()           { return e_stack->p[-1]; }

static stky_array* d_stack;

static stky_v mark;

#define V(i) (v_stack->p[-(i) - 1])

#define stky_f(name) stky_f_##name
#define stky_FD(name) void stky_F_##name(); extern stky_f stky_f(name);
#define stky_F(name) void stky_F_##name(); stky_f stky_f(name) = stky_F_##name; void stky_F_##name()
#define stky_e(name) stky_eval(stky_f(name))

void stky_eval(stky_v v);

stky_F(eval_object) {
}

void stky_callv(const stky_v *v)
{
  while ( v[1] )
    stky_push(*(v ++));
  stky_eval(*v); // TAIL CALL.
}
#define stky_call(...) ({ stky_v _argv[] = { __VA_ARGS__, 0, 0 }; stky_callv(_argv); })

void stky_execv(const stky_v *v)
{
  while ( v[1] )
    stky_eval(*(v ++));
  stky_eval(*v); // TAIL CALL.
}
#define stky_exec(...) ({ stky_v _argv[] = { __VA_ARGS__, 0, 0 }; stky_execv(_argv); })

#define BOP(n,op) \
  stky_F(fx_##n)  \
  { \
  V(1) = stky_v_i(stky_v_i_(V(1)) op stky_v_i_(V(0))); \
  stky_pop(); \
  }
#define UOP(n,op) \
  stky_F(fx_##n)  \
  { \
  V(0) = stky_v_i(op stky_v_i_(V(0))); \
  }
#include "cops.h"

stky_F(type) // v | type(v)
{
  V(0) = stky_v_T_(V(0));
}
stky_F(set_type) // v type | v
{
  stky_v type = stky_pop();
  stky_v v = V(0);
  switch ( stky_v_t_(v) ) {
  case 0:   abort();  break;
  case 1:   abort();  break;
  default:  stky_v_h_(v)->type = type; break;
  }
}
stky_F(nop) { }
stky_F(exec) { stky_eval(stky_pop()); }
stky_F(dup)  { stky_push(V(0)); }
stky_F(pop)  { stky_pop(); }
stky_F(popn) { stky_popn(stky_v_i_(stky_pop())); }
stky_F(exch) { stky_v t = V(0); V(0) = V(1); V(1) = t; }
stky_F(mark) { stky_push(mark); }
stky_inline int stky_v_execQ(stky_v v)
{
  return stky_v_h_(v)->flags & 4;
}
stky_F(set_exec) { // o | o
  stky_v_h_(stky_top())->flags |= 4;
}
stky_F(meta) { // o | m
  V(0) = stky_v_h_(stky_top())->meta;
}
stky_F(set_meta) { // o m | o
  stky_v m = stky_pop();
  stky_v_h_(stky_top())->meta = m;
}
stky_F(v_stack) { stky_push(stky_v_o(v_stack)); }
stky_F(e_stack) { stky_push(stky_v_o(e_stack)); }
stky_F(d_stack) { stky_push(stky_v_o(d_stack)); }
stky_F(count_to_mark) // [ ... mark v1 v2 .. vn ] | n
{
  size_t c = 0;
  stky_array *a = stky_v_o_(stky_pop());
  stky_v* v = a->p;
  while ( -- v > a->b && *v != mark )
    c ++;
  stky_push(stky_v_i(c));
}
stky_F(make_array) // v1 v2 ... vn n | [ v1 v2 ... vn ]
{
  size_t s = stky_v_i_(stky_pop());
  stky_array *a = stky_type_alloc(t_array);
  stky_array_init(a, s);
  for ( size_t i = 0; i < s; ++ i )
    a->b[i] = V(s - 1 - i);
  a->p = a->b + s;
  stky_popn(s);
  stky_push(stky_v_o(a));
}

stky_v array_exec_begin, array_exec_end;
int defer_eval = 0;
stky_F(eval_inner_deferred)
{
  if ( ! defer_eval ) {
    if ( stky_v_T_(stky_top()) == t_symbol )
      stky_e(exec);
    stky_e(exec);
  }
}

stky_F(eval_inner)
{
  stky_v token = stky_top();
  if ( token == array_exec_end ) {
    defer_eval --;
    stky_e(eval_inner_deferred);
  } else if ( token == array_exec_begin ) {
    stky_e(eval_inner_deferred);
    defer_eval ++;
  } else {
    stky_e(eval_inner_deferred);
  }
}

stky_F(eval_array) {
  if ( stky_v_execQ(V(0)) ) {
    stky_array* a = stky_O(stky_pop(), array);
    stky_v *p = a->b;
    if ( p < a->p ) {
      stky_e_push(stky_v_o(a));
      do {
        stky_push(*(p ++));
        stky_e(eval_inner);
      } while ( p < a->p - 1 );
      stky_e_pop();
      stky_push(*p);
      stky_e(eval_inner);
    }
  }
}
stky_F(array_push) { // v array |
  stky_array* a = stky_O(stky_pop(), array);
  stky_v v = stky_pop();
  stky_array_push(a, v);
}
stky_F(array_len) { // [ e0 ... en ] | n - 1
  stky_array* a = stky_O(stky_pop(), array);
  stky_push(stky_v_i(a->p - a->b));
}
stky_F(array_top) { // [ e0 ... en ] | en
  V(0) = stky_O(V(0), array)->p[-1];
}
stky_F(array_get) { // [ e0 ... ] i | ei
  stky_i i = stky_v_i_(stky_pop());
  stky_array* a = stky_O(stky_pop(), array);
  stky_push(a->b[i]);
}

stky_F(array_each) { // [ e0 .. en ] f | e0 f .. en f
  stky_v f = stky_pop();
  stky_array* a = stky_O(stky_pop(), array);
  stky_v *p = a->b;
  while ( p < a->p ) {
    stky_push(*(p ++));
    stky_eval(f);
  }
}

stky_F(cmp_word) {
  stky_v b = stky_pop();
  stky_v a = V(0);
  V(0) = stky_v_i(a < b ? -1 : a == b ? 0 : 1);
}
stky_F(cmp_string) {
  stky_string* b = stky_O(stky_pop(), string);
  stky_string* a = stky_O(stky_pop(), string);
  size_t al = a->p - a->b;
  size_t bl = b->p - b->b;
  int c = memcmp(a->b, b->b, al < bl ? al : bl);
  if ( c == 0 ) c = al < bl ? -1 : al == bl ? 0 : 1;
  stky_push(stky_v_i(c));
}
stky_F(array_to_dict) // [ k1 v1 ... kn vn ] | @[ dict ]@
{
  stky_array* a = stky_v_o_(stky_pop());
  stky_dict* d = stky_type_alloc(t_dict);
  size_t s = a->p - a->b;
  stky_array_init(&d->_, s);
  memcpy(d->_.b, a->b, sizeof(a->p[0]) * s);
  d->_.p += s;
  d->cmp = stky_f(cmp_word);
  stky_push(stky_v_o(d));
}
stky_F(dict_get) { // k dict | v
  stky_dict* d = stky_O(stky_pop(), dict);
  stky_v v = 0;
  stky_v k = stky_pop();
  stky_v *p = d->_.b;
  while ( p < d->_.p ) {
    stky_push(k);
    stky_push(p[0]);
    stky_eval(d->cmp);
    if ( stky_v_i_(stky_pop()) == 0 )
      { v = p[1]; break; }
    p += 2;
  }
  stky_push(v);
}
stky_F(dict_set) { // k v dict | 
  stky_dict* d = stky_O(stky_pop(), dict);
  stky_v v = stky_pop();
  stky_v k = stky_pop();
  stky_v *p = d->_.b;
  while ( p < d->_.p ) {
    stky_call(k, p[0], d->cmp);
    if ( stky_v_i_(stky_pop()) == 0 )
      { p[1] = v; return; }
    p += 2;
  }
  stky_call(k, stky_v_o(d), stky_f(array_push));
  stky_call(v, stky_v_o(d), stky_f(array_push));
}
stky_F(dict_getsert) { // k v dict | v
  stky_v d = stky_pop();
  stky_v v = stky_pop();
  stky_v k = stky_pop();
  stky_call(k, d, stky_f(dict_get));
  if ( ! stky_top() ) {
    stky_pop();
    stky_call(v, k, v, d, stky_f(dict_set));
  }
}
stky_F(dicts_get) { // k dicts | v
  stky_array* ds = stky_O(stky_pop(), array);
  stky_v k = stky_pop();
  stky_v* p = ds->p;
  while ( p > ds->b ) {
    stky_call(k, *(-- p), stky_f(dict_get));
    if ( stky_top() )
      return;
    stky_pop();
  }
  stky_push(0);
}
stky_F(eval_dict) {
  if ( stky_v_execQ(V(0)) ) {
    stky_e(dict_get);
  }
}

stky_F(eval_symbol) { // sym | v
  stky_e(d_stack);
  stky_e(dicts_get);
}

static stky_v sym_dict;
stky_F(sym_dict) { stky_push(sym_dict); }
stky_F(string_to_symbol) { // str | sym
  stky_v sym = stky_v_o(stky_type_alloc(t_symbol));
  stky_O(sym, symbol)->name = V(0);
  stky_call(sym, sym_dict, stky_f(dict_getsert));
}
stky_F(string_push) { // string v |
  int v = stky_v_c_(stky_pop());
  stky_string* self = stky_O(stky_pop(), string);
  if ( self->p >= self->e )
    stky_string_resize(self, self->s * 3 / 2 + 3);
  *(self->p ++) = v;
}
stky_F(charP_to_string) { // char* | string
  stky_push(stky_string_new(stky_pop(), -1));
}
stky_F(string_to_fun) { // str | fun
  stky_string* str = stky_O(stky_pop(), string);
  void *fun = dlsym(RTLD_DEFAULT, str->b);
  stky_push(stky_v_f(fun));
}
stky_F(fun_to_string) { // fun | str
  struct dl_info info = { 0 };
  void *fun = stky_v_f_(stky_pop());
  if ( dladdr(fun, &info) )
    stky_push(stky_string_new(info.dli_sname, -1));
  else
    stky_push(0);
}

stky_v stky_cell_new(stky_v v)
{
  stky_cell* self = stky_type_alloc(t_cell);
  self->value = v;
  return stky_v_o(self);
}
stky_F(eval_cell) { // cell | value(cell)
  V(0) = stky_O(V(0), cell)->value;
}

stky_F(write_string_FILEP)
{
  stky_io* self = stky_O(stky_pop(), io);
  stky_string* str = stky_O(stky_pop(), string);
  fwrite(str->b, sizeof(str->b[0]), str->p - str->b, self->opaque);
}
stky_F(read_char_FILEP) { // IO | c
  stky_io* io = stky_O(stky_pop(), io);
  stky_push(stky_v_c(fgetc(stdin)));
}
stky_F(unread_char_FILEP) { // c IO |
  stky_io* io = stky_O(stky_pop(), io);
  stky_v c = stky_pop();
  ungetc(stky_v_c_(c), stdin);
}
stky_F(at_eos_FILEP) { // IO | 
  stky_io* io = stky_O(stky_pop(), io);
  stky_push(stky_v_i(feof(io->opaque)));
}
stky_F(close_FILEP) { // IO |
  stky_io* io = stky_O(stky_pop(), io);
  if ( io->opaque )
    fclose(io->opaque);
  io->opaque = 0;
}

stky_F(read_char) { // IO | c
  stky_io* io = stky_O(stky_top(), io);
  stky_eval(io->read_char);
}
stky_F(unread_char) { // c IO |
  stky_io* io = stky_O(stky_top(), io);
  stky_eval(io->unread_char);
}
stky_F(at_eos) { // IO | eos?
  stky_io* io = stky_O(stky_top(), io);
  stky_eval(io->at_eos);
}
stky_F(close) { // IO |
  stky_io* io = stky_O(stky_top(), io);
  stky_eval(io->close);
}

stky_v stky_io_new_FILEP(FILE *fp, const char *name, const char *mode)
{
  stky_io *o = stky_type_alloc(t_io);
  if ( ! fp ) { 
    if ( ! (fp = fopen(name, mode)) ) {
      perror("Cannot open file");
    }
  }
  o->opaque = fp;
  o->name         = stky_string_new(name, -1);
  o->mode         = stky_string_new(mode, -1);
  o->write_string = stky_f(write_string_FILEP);
  o->read_char    = stky_f(read_char_FILEP);
  o->unread_char  = stky_f(unread_char_FILEP);
  o->close        = stky_f(close_FILEP);
  o->at_eos       = stky_f(at_eos_FILEP);
  return stky_v_o(o);
}
stky_v stky_stdin, stky_stdout, stky_stderr;

enum read_token_state {
    s_error = -2,
    s_eos = -1,
    s_stop = 0,
    s_start,
    s_comment_eol,
    s_symbol,
    s_literal,
    s_pot_num,
    s_int,
    s_char,
    s_string,
};

#define fgetc(IO) ({ stky_push(stky_v_o(IO)); stky_e(read_char); stky_v_c_(stky_pop()); })
#define ungetc(C,IO) ({ stky_push(stky_v_c(C)); stky_push(stky_v_o(IO)); stky_e(unread_char); })
stky_F(read_token)
{
  stky_io *in = stky_O(stky_pop(), io);
  stky_v value;
  stky_v token = 0;
  stky_i n = 0;
  int pot_num_c = 0;
  int literal = 0;
  int state = s_start, last_state = -1, last_state_2 = s_start;
  int c = -2;
   
#define take_c() c = -2
#define next_c() take_c(); goto again
#define next_s(s) state = (s); goto again
#define stop_s(s) last_state_2 = (s); next_s(s_stop)
 again:
  last_state = last_state_2; last_state_2 = state;
  if ( ! token ) token = stky_string_new("", 0);
  if ( c == -2 ) c = fgetc(in);
  // if ( Y->token_debug >= 2 ) fprintf(stderr, "  c = %s, state = %d, token = '%s'\n", char_to_str(c), state, token->p);
  switch ( state ) {
  case s_error:
    stky_call(token, stky_v_c(c), stky_f(string_push));
    value = token;
  case s_eos:
  case s_stop:
    break;
  case s_start:
    switch ( c ) {
    case EOF: stop_s(s_eos);
    case ' ': case '\t': case '\n': case '\r':
      next_c();
    case '%': take_c(); next_s(s_comment_eol);
    case '/':
      literal ++; next_c();
    case '-': case '+':
      stky_call(token, stky_v_c(c), stky_f(string_push));
      pot_num_c = c; take_c();
      next_s(s_pot_num);
    case '0' ... '9':
      next_s(s_int);
    case '\'':
      state = s_char; next_c();
    case '"':
      state = s_string; next_c();
    default:
      next_s(s_symbol);
    }
  case s_comment_eol:
    switch ( c ) {
    case '\n':
      take_c(); next_s(s_start);
    case EOF:
      next_s(s_start);
    default:
      next_c();
    }
  case s_symbol:
    switch ( c ) {
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      stky_call(token, stky_f(string_to_symbol));
      value = stky_pop(); next_s(s_stop);
    default:
      stky_call(token, stky_v_c(c), stky_f(string_push));
      next_c();
    }
  case s_pot_num:
    switch ( c ) {
    case '0' ... '9':
      next_s(s_int);
    default:
      next_s(s_symbol);
    }
  case s_int:
    switch ( c ) {
    case '0' ... '9':
      stky_call(token, stky_v_c(c), stky_f(string_push));
      {
        stky_i prev_n = n;
        n = n * 10 + c - '0';
        if ( n < prev_n ) { take_c(); next_s(s_symbol); } // overflow
        // fprintf(stderr, "  s_int: n = %lld\n", (long long) n);
      }
      next_c();
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      if ( pot_num_c == '-' ) n = - n;
      value = stky_v_i(n); next_s(s_stop);
    default:
      next_s(s_symbol);
    }
  case s_char:
    switch ( c ) {
    default:
      value = stky_v_c(c); next_s(s_stop);
    }
  case s_string:
    switch ( c ) {
    case '"':
      value = token; take_c(); next_s(s_stop);
    default:
      stky_call(token, stky_v_c(c), stky_f(string_push));
      next_c();
    }
  default:
    abort();
  }
#undef take_c
#undef next_c
#undef next_s
#undef stop_s
 stop:
  if ( c >= 0 ) ungetc(c, in);
  while ( literal -- ) {
    value = stky_cell_new(value);
    last_state = s_literal;
  }
  stky_push(value);
  stky_push(stky_v_i(last_state));

#if 0
  if ( Y->token_debug >= 1 ) {
    stky_io__printf(Y, Y->v_stderr, "  : ");
    stky_write(Y, stderr, value, 1);
    stky_io__printf(Y, Y->v_stderr, " %d\n", last_state);
    stky_print_vs(Y, Y->v_stderr);
  }
  return Y;
#endif
}
#undef fgetc
#undef ungetc

void stky_eval(stky_v v)
{
 again:
  switch ( stky_v_t_(v) ) {
  case 0:  (stky_v_f_(v))();  break; // proc: invoke it.
  case 1:  stky_push(v);      break; // integer: push it.
  case 2:  stky_push(v); stky_e(eval_symbol);  break; // symbol: lookup->exec or push it. 
  default: // other: other...
    stky_push(v);
    v = stky_O(stky_v_T_(v), type)->eval;
    goto again;
  }
}

stky_F(or) { // a b | (a or b)
  stky_v b = stky_pop();
  stky_v a = stky_pop();
  stky_push(a ? a : b);
}
stky_F(and) { // a b | (a and b)
  stky_v b = stky_pop();
  stky_v a = stky_pop();
  stky_push(! a ? a : b);
}

stky_F(write_charP) {
  printf("%s", stky_pop());
}
stky_F(write_string) {
  stky_string* s = stky_O(stky_pop(), string);
  fwrite(s->b, sizeof(s->b[0]), s->p - s->b, stdout);
}
stky_F(print_voidP) {
  printf("%p", stky_pop());
}

stky_F(array_end) { // mark v0 .. vn ] | [ v0 .. vn ]
  stky_exec(stky_f(v_stack), stky_f(count_to_mark),
            stky_f(make_array), stky_f(exch), stky_f(pop));
  
}  
stky_F(array_end_exec) { // mark v0 .. vn | { v0 .. vn }
  stky_exec(stky_f(array_end), stky_f(set_exec)); 
}
stky_F(make_selector) { // default_method
  stky_v default_method = stky_pop();
  stky_exec(stky_f(v_stack), stky_v_i(0),
            stky_f(make_array), stky_f(array_to_dict));
  stky_v methods = stky_pop();
  stky_call((stky_v) 0, stky_pop(), methods, stky_f(dict_set));
  stky_call(mark,
            stky_f(dup),   // obj | obj
            stky_f(type),  // obj | type(obj)
            methods,       // obj | type(obj) methods
            stky_f(dict_get), // obj | method(type)
            0,
            methods,
            stky_f(dict_get), // obj | method(type) default_method
            stky_f(or),  // obj | method
            stky_f(exec), // obj | ...
            stky_f(nop));
  stky_exec(stky_f(array_end_exec));
  stky_v fun = stky_top();
  stky_call(methods, stky_f(set_meta));
}
stky_F(add_method) { // type method selector
  stky_e(meta);
  stky_e(dict_set);
}

stky_v print_methods;
stky_FD(print_object);
stky_F(print) {
  stky_v v = V(0);
  stky_push(v);
  stky_e(type);
  stky_push(print_methods);
  stky_e(dict_get);
  stky_push(stky_f(print_object));
  stky_e(or);
  stky_e(exec);
}
stky_F(print_space) {
  stky_e(print);
  stky_call(" ", stky_f(write_charP));
}
stky_F(print_fun) {
  stky_call("@fun:", stky_f(write_charP));
  stky_e(dup);
  stky_e(fun_to_string);
  if ( stky_top() ) {
    stky_e(write_string);
    stky_pop();
  } else {
    stky_pop();
    stky_e(print_voidP);
  }
}
stky_F(print_fixnum) {
  printf("%lld", (long long) stky_v_i_(stky_pop()));
}
stky_F(print_symbol) {
  stky_call(stky_O(stky_pop(), symbol)->name, stky_f(write_string));
}
stky_F(print_string) {
  stky_call("\"", stky_f(write_charP));
  stky_call(stky_f(write_string));
  stky_call("\"", stky_f(write_charP));
}
stky_F(print_cell) {
  stky_call("/", stky_f(write_charP));
  stky_call(stky_O(stky_pop(), cell)->value, stky_f(print));
}
stky_F(print_type) {
  stky_call("@type:", stky_f(write_charP));
  stky_call(stky_O(stky_pop(), type)->name, stky_f(write_string));
}
stky_F(print_array) {
  int exec = stky_v_execQ(stky_top());
  printf(exec ? "{ " : "[ ");
  stky_call(stky_f(print_space), stky_f(array_each));
  printf(exec ? "}" : "]");
}
stky_F(print_dict) {
  printf("{{ ");
  stky_call(stky_f(print_space), stky_f(array_each));
  printf("}}");
}
stky_F(print_object) {
  stky_e(dup);
  stky_e(type);
  stky_e(print);
  printf(":%p", stky_pop());
}
stky_F(println) {
  stky_e(print);
  stky_call("\n", stky_f(write_charP));
}

stky_F(eval_io) {
  stky_v io = stky_pop();
  int echo_token = 0;
  int print_result = 0;
  
  while ( stky_call(io, stky_f(at_eos)), ! stky_v_i_(stky_pop()) ) {
    stky_call(io, stky_f(read_token));
    stky_v state = stky_pop();
    if ( echo_token ) {
      stky_call(state, stky_f(println));
      stky_e(dup);
      stky_e(println);
    }
    if ( stky_v_i_(state) < 0 ) {
      stky_pop();
      break;
    }
    stky_e(eval_inner);
    if ( print_result ) {
      stky_e(dup);
      stky_e(println);
    }
    // stky_call(sym_dict, stky_f(println));
    stky_call(stky_v_o(v_stack), stky_f(println));
  }
}

stky_v stky_symbol_new(const char* s)
{
  stky_call(stky_string_new(s, -1), stky_f(string_to_symbol));
  return stky_pop();
}

stky_v core_dict;
stky_F(core_dict) { stky_push(core_dict); }

void stky_init()
{
  int id = 0;
  GC_init();
  t_type = stky_v_o(GC_malloc(sizeof(stky_hdr) + sizeof(stky_type)) + sizeof(stky_hdr));
  stky_O(t_type, type)->size = stky_v_i(sizeof(stky_type));
#define TYPE(N)                                                 \
  t_##N = stky_v_o(stky_type_alloc(t_type));                    \
  stky_O(t_type, type)->size = stky_v_i(sizeof(stky_type)); 
#include "types.h"

#define TYPE(N)                                        \
  stky_v_h_(t_##N)->type = t_type;                     \
  stky_O(t_##N, type)->name = stky_string_new(#N, -1);   \
  stky_O(t_##N, type)->size = stky_v_i(sizeof(stky_##N)); \
  stky_O(t_##N, type)->id   = stky_v_i(++ id); \
  stky_O(t_##N, type)->eval = stky_f(eval_object);
#include "types.h"

  stky_O(t_array , type)->eval = stky_f(eval_array);
  stky_O(t_dict  , type)->eval = stky_f(eval_dict);
  stky_O(t_symbol, type)->eval = stky_f(eval_symbol);
  stky_O(t_io,     type)->eval = stky_f(eval_io);
  stky_O(t_cell,   type)->eval = stky_f(eval_cell);

  mark = stky_v_o(stky_type_alloc(t_mark));
  stky_array_init(v_stack = stky_type_alloc(t_array), 1024);
  stky_array_init(e_stack = stky_type_alloc(t_array), 1024);
  stky_array_init(d_stack = stky_type_alloc(t_array), 1024);

  stky_push(stky_v_i(0));
  stky_exec(stky_f(make_array), stky_f(array_to_dict));
  sym_dict = stky_pop();
  stky_O(sym_dict, dict)->cmp = stky_f(cmp_string);

  stky_call(mark,
            t_fun,    stky_f(print_fun),
            t_fixnum, stky_f(print_fixnum),
            t_symbol, stky_f(print_symbol),
            t_string, stky_f(print_string),
            t_type,   stky_f(print_type),
            t_array,  stky_f(print_array),
            t_dict,   stky_f(print_dict),
            t_cell,   stky_f(print_cell),
            stky_f(nop));
  stky_exec(stky_f(v_stack), stky_f(count_to_mark),
            stky_f(make_array), stky_f(array_to_dict),
            stky_f(exch), stky_f(pop));
  print_methods = stky_pop();
  // stky_call(print_methods, stky_f(println));

  stky_stdin  = stky_io_new_FILEP(stdin,  "<stdin>",  "r");
  stky_stdout = stky_io_new_FILEP(stdout, "<stdout>", "w");
  stky_stderr = stky_io_new_FILEP(stderr, "<stderr>", "w");

  stky_push(stky_v_i(0));
  stky_exec(stky_f(make_array), stky_f(array_to_dict));
  core_dict = stky_pop();
  stky_call(core_dict, stky_v_o(d_stack), stky_f(array_push));
  stky_call(stky_symbol_new("["),  mark, core_dict, stky_f(dict_set));
  stky_call(stky_symbol_new("]"), stky_f(array_end), core_dict, stky_f(dict_set));
  stky_call(stky_symbol_new("{{"),  mark, core_dict, stky_f(dict_set));
  stky_call(array_exec_begin = stky_symbol_new("{"), mark, core_dict, stky_f(dict_set));
  stky_call(array_exec_end = stky_symbol_new("}"), stky_f(array_end_exec), core_dict, stky_f(dict_set));
#define F(N) \
  stky_call(stky_symbol_new("&" #N), stky_f(N), core_dict, stky_f(dict_set));
  F(type);
  F(pop);
  F(popn);
  F(dup);
  F(exch);
  F(exec);
  F(array_len);
  F(array_get);
  F(dict_get);
  F(dict_set);
  F(dict_getsert);
  F(sym_dict);
  F(d_stack);
  F(e_stack);
  F(core_dict);
#define BOP(n,op) F(fx_##n);
#define UOP(n,op) F(fx_##n);
#include "cops.h"

  // NON-CORE:
  F(print);
  F(println);
#undef F
  stky_call(stky_symbol_new("x"), stky_v_i(123), core_dict, stky_f(dict_set));
  stky_call(core_dict, stky_f(println));
}

int main(int argc, char **argv, char **envp)
{
  stky_init();
#if 0
  stky_call(stky_v_i(123), stky_f(println));
  stky_call(stky_f(print), stky_f(println));
  stky_call(stky_v_i(0), mark, stky_v_i(1), stky_v_i(2), stky_f(v_stack));
  stky_exec(stky_f(count_to_mark));
  stky_exec(stky_f(dup), stky_f(println));
  stky_exec(stky_f(make_array), stky_f(println));
#endif
  stky_call(stky_stdin, stky_f(eval_io));
  return 0;
}
