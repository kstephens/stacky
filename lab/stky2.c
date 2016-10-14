#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>

#define stky_inline static __inline__ __attribute__((always_inline))

struct stky_object;
typedef struct stky_object stky_object;

typedef void *stky_v;
typedef intptr_t stky_i;
typedef void (*stky_f)();
typedef void *stky_o;

#define TYPE(N) stky_v t_##N; struct stky_##N; typedef struct stky_##N stky_##N;
#include "types.h"
struct stky_object {
  stky_v type;
  size_t flags;
};

struct stky_type {
  stky_object _;
  stky_v name;
  stky_v size;
  stky_v id;
};

struct stky_fun {
  stky_object _;
};

struct stky_fixnum {
  stky_object _;
};

struct stky_array {
  stky_object _;
  size_t l, s;
  stky_v *b, *v, *e;
};

struct stky_string {
  stky_object _;
  size_t l, s;
  char *b, *v, *e;
};

struct stky_symbol {
  stky_object _;
  stky_v name;
};

struct stky_mark {
  stky_object _;
};

stky_inline int       stky_v_t_(stky_v v)   { return ((stky_i) (v)) & 3; }
stky_inline stky_v    stky_v_f (stky_f v)   { return  (stky_v) v; }
stky_inline stky_f    stky_v_f_(stky_v v)   { return  (stky_f) v; }
stky_inline stky_v    stky_v_i (stky_i v)   { return  (stky_v) (((v) << 2) + 1) ; }
stky_inline stky_i    stky_v_i_(stky_v v)   { return ((stky_i)  (v)) >> 2       ; }
stky_inline stky_v    stky_v_o (stky_o v)   { return ((void*) v) + 3; }
stky_inline stky_o    stky_v_o_(stky_v v)   { return ((void*) v) - 3; }
#define stky_O(v,type) ((stky_##type*) stky_v_o_(v))

stky_inline int stky_o_executableQ(stky_v v) {
  return stky_O(v, object)->flags & 1;
}

void stky_array_resize(stky_array *self, size_t s)
{
  stky_v *v = realloc(self->b, sizeof(self->v[0]) * (s + 1));
  self->v = v + (self->v - self->b);
  self->b = v;
  self->e = v + s;
  self->s = s;
  self->b[self->s] = 0;
}
stky_array* stky_array_init(stky_array* self, size_t s)
{
  self->_.type = t_array;
  self->_.flags = 0;
  self->b = self->v = self->e = 0;
  self->l = self->s = 0;
  stky_array_resize(self, s);
  return self;
}
void stky_array_push(stky_array *self, stky_v v)
{
  if ( ++ self->v >= self->e )
    stky_array_resize(self, self->s * 3 / 2 + 3);
  *self->v = v;
}
stky_v stky_array_pop(stky_array *self)
{
  if ( self->v <= self->b )
    abort();
  return *(self->v --);
}

void stky_string_resize(stky_string *self, size_t s)
{
  char *v = realloc(self->b, sizeof(self->v[0]) * (s + 1));
  self->v = v + (self->v - self->b);
  self->b = v;
  self->e = v + s;
  self->s = s;
  self->b[self->s] = 0;
}
stky_string* stky_string_init(stky_string* self, size_t s)
{
  self->_.type = t_string;
  self->_.flags = 0;
  self->b = self->v = self->e = 0;
  self->l = self->s = 0;
  stky_string_resize(self, s);
  return self;
}
stky_string* stky_string_new(int s, const char *v)
{
  if ( s < 0 ) s = strlen(v);
  stky_string* self = malloc(sizeof(*self));
  stky_string_init(self, s);
  memcpy(self->b, v, sizeof(self->b[0]) * s);
  self->l = s;
  self->v = self->b + s;
  return self;
}

static stky_array v_stack;
stky_inline void   stky_push(stky_v v)    { *(++ v_stack.v) = v; }
stky_inline stky_v stky_pop()             { return *(v_stack.v --); }
stky_inline void   stky_popn(size_t n)    { v_stack.v -= n; }
stky_inline stky_v stky_top()             { return *v_stack.v; }

static stky_array e_stack;
int stky_defer_exec;
stky_inline void   stky_e_push(stky_v v)  { *(++ e_stack.v) = v; }
stky_inline stky_v stky_e_pop()           { return *(e_stack.v --); }

static stky_array d_stack;

static stky_v mark;

#define V(i) (v_stack.v[-(i)])

#define stky_f(name) stky_f_##name
#define stky_F(name) void stky_F_##name(); stky_f stky_f(name) = stky_F_##name; void stky_F_##name()
#define stky_e(name) (stky_f(name)(), stky_top())

stky_F(type)
{
  stky_v v = V(0);
  switch ( stky_v_t_(v) ) {
  case 0:   v = t_fun; break;
  case 1:   v = t_fixnum; break;
  case 2:   v = t_symbol; break;
  default:  v = stky_O(v, object)->type; break;
  }
  V(0) = v;
}
stky_F(dup)  { stky_push(V(0)); }
stky_F(pop)  { stky_pop(); }
stky_F(exch) { stky_v t = V(0); V(0) = V(1); V(1) = t; }
stky_F(mark) { stky_push(mark); }
stky_F(v_stack) { stky_push(stky_v_o(&v_stack)); }
stky_F(e_stack) { stky_push(stky_v_o(&e_stack)); }
stky_F(d_stack) { stky_push(stky_v_o(&d_stack)); }
stky_F(count_to_mark) // [ ... mark v1 v2 .. vn ] | n
{
  size_t c = 0;
  stky_array *a = stky_v_o_(stky_pop());
  stky_v* v = a->v;
  while ( *v != mark && v -- > a->b ) c ++;
  stky_push(stky_v_i(c));
}
stky_F(make_array) // v1 v2 ... vn n | [ v1 v2 ... vn ]
{
  size_t s = stky_v_i_(stky_pop());
  stky_array *a = malloc(sizeof(*a));
  stky_array_init(a, s);
  for ( size_t i = 0; i < s; ++ i )
    stky_array_push(a, V(s - i));
  stky_popn(s);
  stky_push(stky_v_o(a));
}

void stky_eval(stky_v v);

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


stky_F(exec) { stky_exec(stky_pop()); }
stky_F(exec_array) {
  if ( stky_o_executableQ(V(0)) ) {
    stky_v o = stky_pop();
    stky_v *v = stky_O(o, array)->b;
    size_t  i = stky_O(o, array)->v - v;
    if ( i ) {
      stky_e_push(o);
      while ( -- i )
        stky_exec(*v ++);
      stky_e_pop();
      stky_exec(*v); // TAIL CALL.
    }
  }
}
stky_F(array_push) { // array v |
  stky_v v = stky_pop();
  stky_array* a = stky_O(stky_pop(), array);
  stky_array_push(a, v);
}
stky_F(dict_get) { // dict k | v
  stky_v k = stky_pop();
  stky_array* d = stky_O(stky_pop(), array);
  stky_v v = 0;
  stky_v *p = d->b;
  while ( p < d->v ) {
    if ( p[0] == k ) { v = p[1]; break; }
    p += 2;
  }
  stky_push(v);
}
stky_F(dict_set) { // dict k v | 
  stky_v v = stky_pop();
  stky_v k = stky_pop();
  stky_array* d = stky_O(stky_pop(), array);
  stky_v *p = d->b;
  while ( p < d->v ) {
    if ( p[0] == k ) { p[1] = v; return; }
    p += 2;
  }
  stky_call(d, k, stky_f(array_push), d, stky_f(array_push));
}

stky_F(dicts_get) { // k dicts | v
  stky_array* ds = stky_O(stky_pop(), array);
  stky_v k = stky_pop();
  stky_v* p = ds->v;
  while ( p >= ds->b ) {
    if ( *p ) {
      stky_call(*p, k, stky_f(dict_get));
      if ( stky_top() )
        break;
    }
    -- p;
  }
  abort();
}
stky_F(exec_symbol) { // sym | v
  if ( ! stky_defer_exec ) {
    stky_e(d_stack);
    stky_e(dicts_get);
  }
}
stky_F(exec_object) {
  stky_push(stky_top());
}

stky_v print_methods;
stky_F(print_fun) {
  stky_v v = stky_pop();
  printf("fun:%p", stky_v_f_(v));
}
stky_F(print_integer) {
  stky_v v = stky_pop();
  printf("%lld", (long long) stky_v_i_(v));
}
stky_F(print_symbol) {
  stky_v v = stky_pop();
  printf("%s", stky_O(stky_O(v, symbol)->name, string)->b);
}
stky_F(print_other) {
  stky_v v = stky_pop();
  printf("%p:%p", stky_O(v, object)->type, v);
}
stky_F(or) { // a b | (a or b)
  stky_v b = stky_pop();
  stky_v a = stky_pop();
  stky_push(a ? a : b);
}
stky_F(print) {
  stky_v v = stky_pop();
  stky_push(print_methods);
  stky_push(v);
  stky_e(type);
  stky_e(dict_get);
  stky_push(stky_f(print_other));
  stky_e(or);
  stky_push(v);
  stky_e(exec);
}

void stky_eval(stky_v v)
{
  switch ( stky_v_t_(v) ) {
  case 0:  (stky_v_f_(v))();  break; // proc: invoke it.
  case 1:  stky_push(v);      break; // integer: push it.
  case 2:  stky_push(v); stky_eval(stky_f(exec_symbol));  break; // symbol: lookup->exec or push it. 
  default: stky_push(v); stky_eval(stky_f(exec_object));  break; // other: other...
  }
}

void stky_init()
{
  int id = 0;
#define TYPE(N) t_##N = stky_v_o(malloc(sizeof(stky_type)));
#include "types.h"  
#define TYPE(N) \
  stky_O(t_##N, type)->name = stky_v_o(stky_string_new(-1, #N)); \
  stky_O(t_##N, type)->size = stky_v_i(sizeof(stky_##N)); \
  stky_O(t_##N, type)->id   = stky_v_i(++ id);
#include "types.h"
  mark = stky_v_o(malloc(sizeof(mark)));
  stky_array_init(&v_stack, 1024);
  stky_array_init(&e_stack, 1024);
  stky_array_init(&d_stack, 1024);
}

int main(int argc, char **argv, char **envp)
{
  stky_init();
  stky_call(stky_v_i(123), stky_f(print));
  printf("\n");
  stky_call(stky_f(print), stky_f(print));
  printf("\n");
  stky_call(stky_v_i(1), mark, stky_v_i(1), stky_f(v_stack));
  stky_exec(stky_f(count_to_mark));
  stky_exec(stky_f(dup), stky_f(print));
  stky_exec(stky_f(make_array), stky_f(print));
  printf("\n");
  return 0;
}
