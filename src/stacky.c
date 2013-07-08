#include "stacky/stacky.h"
#include "gc/gc.h"
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include <dlfcn.h>

static stky_isn isn_defs[] = {
#define ISN(name,nwords) { { 0 }, -1, isn_##name, nwords, #name, "&&" #name },
#include "stacky/isns.h"
#undef ISN
  { 0, 0, 0, 0 },
};

#ifdef stky_NO_GC
#define stky_malloc(S)    malloc(S)
#define stky_realloc(P,S) realloc(P,S)
#define stky_free(P)      free(P)
#define GC_init()         ((void) 0)
#else
#ifdef stky_DEBUG_ALLOC
void *stky_malloc_(size_t s, const char *file, int line)
{
  void *p = GC_malloc(s);
  // fprintf(stderr, "  stky_malloc(%lu) => %p     \t %s:%d\n", (unsigned long) s, p, file, line);
  return p;
}
void *stky_realloc_(void *o, size_t s, const char *file, int line)
{
  void *p = GC_realloc(o, s);
  // fprintf(stderr, "  stky_realloc(%p, %lu) => %p \t %s:%d\n", o, (unsigned long) s, p, file, line);
  return p;
}
void stky_free_(void *p, const char *file, int line)
{
  GC_free(p);
  // fprintf(stderr, "  stky_malloc(%lu) => %p     \t %s:%d\n", (unsigned long) s, p, file, line);
}
#define stky_malloc(S)    stky_malloc_(S, __FILE__, __LINE__)
#define stky_realloc(P,S) stky_realloc_(P,S, __FILE__, __LINE__)
#define stky_free(P)      stky_free_(P, __FILE__, __LINE__)
#else
#define stky_malloc(S)    GC_malloc(S)
#define stky_realloc(P,S) GC_realloc(P,S)
#define stky_free(P)      GC_free(P)
#endif
#endif

stky_v stky_object_init(stky *y, void *p, stky_type *type, size_t size)
{
  stky_object *o = p;
  assert(stky_v_tag(o) == 0);
  o->type = type;
  o->flags = 0;
  o->size = size;
  return o;
}

stky_v stky_object_new(stky *Y, stky_type *type, size_t size)
{
  stky_object *o = stky_malloc(size);
  stky_object_init(Y, o, type, size);
  // fprintf(stderr, "  stky_object_new(%p, %p %s, %lu) => %p\n", Y, type, type ? type->name : "", (unsigned long) size, o);
  return o;
}

stky_v stky_object_dup(stky *Y, stky_v v)
{
  stky_object *a = v;
  stky_object *b = stky_malloc(a->size);
  memcpy(b, a, a->size);
  return b;
}

stky_v stky_literal_new(stky *Y, stky_v v)
{
  stky_literal *o = stky_object_new(Y, stky_t(literal), sizeof(*o));
  o->value = v;
  return o;
}

stky_v stky_voidP__new(stky *Y, void *v)
{
  stky_literal *o = stky_object_new(Y, stky_t(voidP), sizeof(*o));
  o->value = v;
  return o;
}

stky_bytes *stky_bytes_init(stky *Y, stky_bytes *a, size_t es, size_t s)
{
  size_t cs = (a->es = es) * ((a->s = s) + 1);
  a->p = a->b = stky_malloc(cs);
  memset(a->b, 0, cs); // clear + null terminator
  return a;
}

stky_bytes *stky_bytes_dup(stky *Y, stky_bytes *a)
{
  ssize_t p_off = a->p - a->b;
  stky_bytes *b = stky_object_dup(Y, a);
  stky_bytes_init(Y, b, a->es, a->s);
  memcpy(b->b, a->b, a->es * (a->s + 1));
  b->p = b->b + p_off;
  return b;
}

stky *stky_bytes_resize(stky *Y, stky_bytes *a, size_t s)
{
  stky_v *nb;
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  nb = stky_realloc(a->b, a->es * (s + 1));
  a->s = s;
  if ( a->l > a->s ) a->l = a->s;
  a->p = nb + (a->p - a->b);
  a->b = nb;
  void *e = a->b + a->es * a->s;
  if ( a->p > e ) a->p = e;
  memset(e, 0, a->es); // null terminator
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  return Y;
}

stky *stky_bytes_append(stky *Y, stky_bytes *a, const void *p, size_t s)
{
  if ( a->l + s > a->s ) stky_bytes_resize(Y, a, a->l + s);
  memmove(((void*) a->p) + a->es * a->l, p, a->es * s);
  a->l += s;
  memset( ((void*) a->p) + a->es * a->l, 0, a->es);
  return Y;
}

stky_array *stky_array_init(stky *Y, stky_array *a, size_t s)
{
  stky_bytes_init(Y, (void*) a, sizeof(a->p[0]), s);
  a->l = 0;
  return a;
}
stky_array *stky_array__new(stky *Y, const stky_v *p, size_t s)
{
  stky_array *o = (void*) stky_object_new(Y, stky_t(array), sizeof(*o));
  stky_array_init(Y, o, s);
  if ( p ) memcpy(o->p, p, sizeof(o->p[0]) * s);
  o->l = s;
  return o;
}

stky_words *stky_words_init(stky *Y, stky_words *a, size_t s)
{
  stky_bytes_init(Y, (void*) a, sizeof(a->p[0]), s);
  return a;
}

stky_words *stky_words_new(stky *Y, const stky_i *p, size_t s)
{
  stky_words *o = stky_object_new(Y, stky_t(words), sizeof(*o));
  stky_words_init(Y, (void*) o, s);
  if ( p ) memcpy(o->p, p, sizeof(o->p[0]) * s);
  o->l = s;
  o->o.flags |= 1;
  o->name = 0;
  return o;
}

stky_string *stky_string_new_charP(stky *Y, const char *p, size_t s)
{
  stky_string *o;
  if ( s == (size_t) -1 ) s = strlen(p);
  o = stky_object_new(Y, stky_t(string), sizeof(*o));
  stky_bytes_init(Y, (void*) o, sizeof(o->p[0]), s);
  o->o.type = stky_t(string);
  if ( p ) memcpy(o->p, p, sizeof(p[0]) * s);
  o->l = s;
  return o;
}

stky *stky_string_append(stky *Y, stky_string *o, const void *p, size_t s)
{
  stky_array *a = (void*) o;
  if ( s == (size_t) -1 ) s = strlen(p);
  return stky_bytes_append(Y, (void*) a, p, s);
}

stky *stky_string_append_char(stky *Y, stky_string *o, int c)
{
  char p[] = { c };
  return stky_string_append(Y, o, p, 1);
}

stky_v stky_array_top(stky *Y, stky_array *o)
{
  if ( o->l <= 0 ) abort();
  return o->p[o->l - 1];
}
stky_v stky_array_top_(stky *Y, stky_array *o, stky_v v)
{
  if ( o->l <= 0 ) abort();
  return o->p[o->l - 1] = v;
}
stky_v* stky_array_popn(stky *Y, stky_array *o, size_t s)
{
  if ( o->l < s ) abort();
  return o->p + (o->l -= s);
}
stky_v stky_array_pop(stky *Y, stky_array *o)
{
  return *stky_array_popn(Y, o, 1);
}
stky_v stky_array_push(stky *Y, stky_array *o, stky_v v)
{
  if ( o->l >= o->s ) stky_bytes_resize(Y, (void*) o, o->s + 1024);
  return o->p[o->l ++] = v;
}

stky_catch *stky_catch__new(stky *Y)
{
  stky_catch *c = stky_object_new(Y, stky_t(catch), sizeof(*c));
  c->prev = Y->current_catch; // NOT THREAD-SAFE
  Y->current_catch = c; // NOT THREAD-SAFE
  c->thrown = stky_v_int(0);
  c->value = 0;
  c->vs_depth = stky_v_int(Y->vs.l);
  c->es_depth = stky_v_int(Y->es.l);
  c->prev_error_catch = Y->error_catch; // NOT THREAD-SAFE
  c->defer_eval = c->defer_eval;
  return c;
}

void stky_catch__throw(stky *Y, stky_catch *c, stky_v value)
{
  {
    stky_catch *o = Y->current_catch;
    while ( o && o != c ) {
      o->thrown = stky_v_int(2); // ABORTED
      o = o->prev;
    }
  }
  Y->current_catch = c->prev;
  if ( stky_v_int_(c->thrown) ) abort();
  c->value  = value;
  c->thrown = stky_v_int(1);
  Y->vs.l = stky_v_int_(c->vs_depth);
  Y->es.l = stky_v_int_(c->es_depth);
  Y->error_catch = c->prev_error_catch;
  Y->defer_eval = c->defer_eval;
  siglongjmp(c->jb, 1);
}

stky_io *stky_io__new_string(stky *Y, stky_string *s)
{
  stky_io *o = stky_object_new(Y, stky_t(io), sizeof(*o));
  if ( ! s ) s = stky_string_new_charP(Y, 0, 0);
  o->opaque = s;
  o->name = o->mode = 0;
  o->write_string = (stky_v) stky_isn_w(isn_write_string_string);
  o->read_char    = (stky_v) stky_isn_w(isn_read_char_string);
  o->unread_char  = (stky_v) stky_isn_w(isn_unread_char_string);
  o->close        = (stky_v) stky_isn_w(isn_close_string);
  o->at_eos       = (stky_v) stky_isn_w(isn_at_eos_string);
  return o;
}

stky_io *stky_io__new_FILEP(stky *Y, FILE *fp, const char *name, const char *mode)
{
  stky_io *o = stky_object_new(Y, stky_t(io), sizeof(*o));
  if ( ! fp ) { 
    if ( ! (fp = fopen(name, mode)) ) {
      perror("Cannot open file");
    }
  }
  o->opaque = fp;
  o->name = stky_string_new_charP(Y, name, -1);
  o->mode = stky_string_new_charP(Y, mode, -1);
  o->write_string = (stky_v) stky_isn_w(isn_write_string_FILEP);
  o->read_char    = (stky_v) stky_isn_w(isn_read_char_FILEP);
  o->unread_char  = (stky_v) stky_isn_w(isn_unread_char_FILEP);
  o->close        = (stky_v) stky_isn_w(isn_close_FILEP);
  o->at_eos       = (stky_v) stky_isn_w(isn_at_eos_FILEP);
  return o;
}

stky_v stky_push(stky *Y, stky_v v);

void stky_io__write_string(stky *Y, stky_io *io, const char *p, size_t s)
{
  stky_string o;
  if ( s == -1 ) s = strlen(p);
  stky_object_init(Y, &o, stky_t(string), sizeof(o));
  o.p = o.b = (char *) p; o.l = s; o.s = s + 1; o.es = sizeof(p[0]);
  stky_push(Y, &o); stky_push(Y, io); stky_exec(Y, isn_write_string);
}

void stky_io__printf(stky *Y, stky_io *io, const char *fmt, ...)
{
  char buf[1024];
  va_list va; va_start(va, fmt); vsnprintf(buf, sizeof(buf) -1, fmt, va); va_end(va);
  stky_io__write_string(Y, io, buf, strlen(buf));
}

void stky_io__close(stky *Y, stky_io *io)
{
  stky_push(Y, io); stky_exec(Y, isn_close);
}

int stky_io__at_eos(stky *Y, stky_io *io)
{
  stky_push(Y, io); stky_exec(Y, isn_at_eos);
  return stky_v_int_(stky_pop(Y));
}

static
void stky_print_vs(stky *Y, stky_io *out)
{
  size_t i = 0;
  stky_io__write_string(Y, out, "  # V: ", -1);
  while ( i < Y->vs.l ) {
    stky_io__write(Y, out, Y->vs.p[i ++], 2);
    stky_io__write_string(Y, out, " ", 1);
  }
  stky_io__write_string(Y, out, "| \n", 3);
}

#define fprintf(FP, FMT...) stky_io__printf(Y, Y->v_##FP, ##FMT)
#define stky_write(Y, FP, V, N) stky_io__write(Y, Y->v_##FP, V, N)

void stky_dict__rehash(stky *Y, stky_dict *o)
{
  if ( o->a.o.flags & 2 ) return;
  o->a.o.flags |= 2;
}

stky_v stky_top(stky *Y) { return stky_array_top(Y, &Y->vs); }
stky_v stky_top_(stky *Y, stky_v v) { return stky_array_top_(Y, &Y->vs, v); }
stky_v stky_push(stky *Y, stky_v v) { 
  if ( Y->trace > 0 ) { fprintf(stderr, "  # << "); stky_write(Y, stderr, v, 1); fprintf(stderr, "\n"); }
  return stky_array_push(Y, &Y->vs, v); }
stky_v stky_pop(stky *Y) { 
  if ( Y->trace > 0 ) { fprintf(stderr, "  # >> "); stky_write(Y, stderr, stky_top(Y), 1); fprintf(stderr, "\n"); }
  return stky_array_pop(Y, &Y->vs); }
stky_v *stky_popn(stky *Y, size_t s ) {
  if ( Y->trace > 0 ) { fprintf(stderr, "  # << # %lu\n", s); }
  return stky_array_popn(Y, &Y->vs, s); }

stky *stky_call(stky *Y, stky_i *pc)
{
  size_t es_i = Y->es.l;
  stky_v val;

#define VS_SAVE()
#define VS_RESTORE()

#define PUSH(X) stky_push(Y, (X))
#define PUSHt(X,T) PUSH((stky_v) (X))
#define POP() stky_pop(Y)
#define POPN(N) stky_popn(Y, (N))
#define swap(a,b) ({ __typeof__(a) __tmp##__LINE__ = a; a = b; b = __tmp##__LINE__; })
#define SWAP(a,b) { val = V(a); V(a) = V(b); V(b) = val; }
#define vp (Y->vs.p + (Y->vs.l - 1))
#define V(i) vp[- (i)]
#define Vt(i,t) (*((t*) (vp - (i))))
#define Vi(i) stky_v_int_(V(i))
#define CALL(P) do {                               \
    stky_array_push(Y, &Y->es, (stky_v) pc);       \
    pc = (stky_i*) (P);                            \
    goto call;                                     \
  } while (0)
#define CALLISN(I) do {                         \
    stky_i save = Y->defer_eval;                                \
    Y->defer_eval = 0;                                          \
    if ( 0 ) { fprintf(stderr, "  # CALLISN(%s)\n", #I); }      \
    Y->trace --;                                \
    stky_call(Y, Y->isns[I].words->p);        \
    Y->trace ++;                                \
    Y->defer_eval = save;                       \
  } while (0)

 call:
  if ( Y->trace > 0 ) { fprintf(stderr, "  # {\n"); }
#if 0
  if ( ! Y->isns[0].addr ) {
    struct isn_def *isn;
#define ISN(name,lits) isn = &Y->isns[isn_##name]; isn->addr = &&I_##name;
#include "stacky/isns.h"
    Y->isns[0].addr = (void*) i;
  }

  if ( Y->threaded_comp && *pc == isn_hdr ) {
    stky_i *pc_save = pc;
    *(pc ++) = isn_hdr_;
    while ( *pc != isn_END ) {
      if ( ! stky_v_isnQ(*pc) ) { pc ++; continue; }
      struct isn_def *isn = &Y->isns[*pc];
      fprintf(stderr, "  %4d %10s => @%p\n", (int) (pc - pc_save), isn->name, isn->addr);
      *pc = (stky_i) isn->addr;
      pc += isn->nwords;
    }
    *pc = (stky_i) &abort;
    pc = pc_save;
    pc ++;
    goto *((void**) (pc ++));
  }
#endif

 next_isn:
#if 1
  if ( Y->trace > 0 ) {
    { 
      stky_i **p = (stky_i **) Y->es.p;
      fprintf(stderr, "\n  # E: ");
      while ( p < (stky_i **) (Y->es.p + Y->es.l) )
        fprintf(stderr, "@%p ", (void*) *(p ++));
      fprintf(stderr, "| \n");
    }
    stky_print_vs(Y, Y->v_stderr);
    switch ( stky_v_type_i(*pc) ) {
    case stky_t_isn:
    if ( *pc != isn_END ) {
      fprintf(stderr, "  # I: %-20s @%p defer_eval:%d", Y->isns[*pc].name, (void*) pc, (int) Y->defer_eval);
      switch ( *pc ) {
      case isn_lit: case isn_lit_voidP:
        fprintf(stderr, "%p ", (void*) pc[1]); break;
      case isn_lit_int:
        fprintf(stderr, "%lld ", (long long) pc[1]); break;
      case isn_lit_charP: case isn_sym_charP:
        fprintf(stderr, "\"%s\" ", (char*) pc[1]); break;
      case isn_ifelse:
        fprintf(stderr, "@%p @%p ", (void*) pc[1], (void*) pc[2]); break;
      case isn_ifelser:
        fprintf(stderr, "@%p @%p ", (void*) pc + 2 + pc[1], (void*) pc + 2 + pc[2]); break;
      }
    } else {
      fprintf(stderr, "  # I: %p ", *((void**) pc));
    }
    break;
    default:
      fprintf(stderr, "  # <: ");
      stky_write(Y, stderr, (stky_v) *pc, 1); 
    }
    fprintf(stderr, "\n");
  }
#endif

  val = (stky_v) *(pc ++);
  eval:
  switch ( (stky_i) val ) {
  default:
    if ( Y->defer_eval > 0 ) {
      if ( val == Y->s_marke )     Y->defer_eval ++; else
      if ( val == Y->s_array_tme ) Y->defer_eval --;
    } else {
      if ( val == Y->s_marke && ! Y->defer_eval ++ ) { 
        PUSH(Y->v_marke); goto next_isn;
      }
    }
    if ( Y->defer_eval > 0 ) {
      PUSH(val); goto next_isn;
    } else {
#define TYPE(name) goto next_isn; case stky_t_##name: T_##name
      switch ( stky_v_type_i(val) ) {
      default:       PUSH(val);
        //      TYPE(null):    goto I_rtn;
      TYPE(isn):     fprintf(stderr, "\nFATAL: invalid isn %lld\n", (long long) val); abort();
      TYPE(string):
        PUSH(val);
        if ( ((stky_object*) val)->flags & 1 ) {
          stky_io *sio;
          CALLISN(isn_object_dup); val = POP();
          sio = stky_io__new_string(Y, val);
          stky_io__eval(Y, sio);
        }
      TYPE(io):
        if ( ((stky_object*) val)->flags & 1 ) {
          stky_io__eval(Y, val);
        } else {
          PUSH(val);
        }
      TYPE(literal): PUSH(((stky_literal*) val)->value);
      TYPE(symbol):  PUSH(val); stky_exec(Y, isn_lookup, isn_eval);
      TYPE(array):
        if ( ((stky_object*) val)->flags & 1 ) {
        call_array:
          CALL(((stky_array*) val)->p);
        } else {
          PUSH(val);
        }
      TYPE(words):
        // fprintf(stderr, "  # words @%p ", val); stky_write(Y, stderr, val, 2); fprintf(stderr, "\n");
        goto call_array;
      }
#undef TYPE
    }
    abort();

  case 0: goto I_rtn;
// #include "isns.c"
#ifdef ISN_DEF
#define ISN(name,nwords) ISN(name,nwords)
#else
#define ISN(name,nwords) goto next_isn; case isn_##name: I_##name
#endif
  ISN(nul,1):   abort();
  ISN(hdr,1):
  ISN(hdr_,1):
  ISN(nop,1):
  ISN(lit,2):       PUSH((stky_v) *(pc ++));
  ISN(lit_int,2):   PUSH(stky_v_int(*(pc ++)));
  ISN(lit_charP,2):
    val = stky_string_new_charP(Y, (void*) *(pc ++), -1);
    pc[-2] = isn_lit; pc[-1] = (stky_i) val;
    PUSH(val);
  ISN(lit_voidP,2): PUSHt(*(pc ++),voidP);
  ISN(eqw,1): {
      V(1) = stky_v_int(V(1) == V(0));
      POP();
    }
  ISN(string_eq,1): {
      stky_string *a = V(1), *b = V(0);
      POP();
      V(0) = stky_v_int(a == b || (a->l == b->l && ! memcmp(a->p, b->p, a->l)));
    }
  ISN(eqw_hsh,1): {
      stky_w h = 0xdeadbeef;
#define HSH(V) h ^= (stky_w) (V); h ^= h >> 3; h ^= h << 5; h ^= h >> 7; h ^= h << 11;
      HSH(V(0));
      V(0) = stky_v_int(h >> 1);
    }
  ISN(string_eq_hsh,1): {
      stky_string *a = V(0);
      stky_w h = 0xdeadbeef;
      unsigned char *p = (void*) a->p, *e = p + a->l;
      while ( p < e ) {
        HSH(*(p ++));
      }
      V(0) = stky_v_int(h >> 1);
#undef HSH
    }
  ISN(vget,1): // ... 2 1 0 n=1 VGET | 1
    V(0) = V(Vi(0) + 1);
  ISN(vset,1): // ... 2 1 0 v n=1 VSET | ... 2 v 0 
    V(Vi(0) + 2) = V(1); POPN(2);
  ISN(dup,1):   PUSH(V(0));
  ISN(pop,1):   POP();
  ISN(popn,1):  val = POP(); POPN(stky_v_int_(val));
  ISN(exch,1):  SWAP(0, 1);
  ISN(rotl,1): { 
      size_t n = Vi(0); POP();
      // 3 2 1 0 n | 2 1 0 3 : n = 4
      //       ^
      stky_v tmp = V(n - 1);
      memmove(&V(n - 1), &V(n - 2), sizeof(V(0)) * (n - 1));
      V(0) = tmp;
    }
#define BOP(name,op) ISN(name,1): V(1) = stky_v_int(Vi(1) op Vi(0)); POP();
#define UOP(name,op) ISN(name,1): V(0) = stky_v_int(op Vi(0));
#include "stacky/cops.h"
#define BOP(N,OP) ISN(w##N,1): V(1) = (stky_v) ((stky_w) V(1) OP (stky_w) V(0)); POP();
#define UOP(N,OP) ISN(w##N,1): V(0) = (stky_v) (OP (stky_w) V(0));
#include "stacky/cops.h"
#define BOP(N,OP) ISN(i##N,1): V(1) = (stky_v) ((stky_i) V(1) OP (stky_i) V(0)); POP();
#define UOP(N,OP) ISN(i##N,1): V(0) = (stky_v) (OP (stky_i) V(0));
#include "stacky/cops.h"
  ISN(ifelse,1): // v t f IFELSE | (t|f)
    if ( Vi(2) ) {
      POP(); SWAP(0, 1); POP(); CALLISN(isn_eval);
    } else {
      SWAP(0, 1); POP(); SWAP(0, 1); POP(); CALLISN(isn_eval);
    }
  ISN(loop,1): // e t LOOP |
  loop_again:
    PUSH(V(0)); CALLISN(isn_eval);
    if ( V(0) ) { POP(); PUSH(V(1)); CALLISN(isn_eval); goto loop_again; }
    POPN(3);
  ISN(ifelser,3):
    pc += 2;
    pc[-3] = isn_ifelse;
    pc[-2] = (stky_i) (pc + pc[-2]);
    pc[-1] = (stky_i) (pc + pc[-1]);
    pc -= 2;
    goto I_ifelsej;
  ISN(ifelsej,3):
    if ( Vi(0) ) {
      POP(); pc = ((stky_i**) pc)[0];
    } else {
      POP(); pc = ((stky_i**) pc)[1];
    }
  ISN(jmpr,2):
    ++ pc;
    pc[-2] = isn_jmp;
    pc[-1] = (stky_i) (pc + pc[-1]);
    pc = (stky_i*) pc[-1];
  ISN(jmp,2):
    pc = (stky_i*) pc[0];
  ISN(v_stdin,1):  PUSH(Y->v_stdin);
  ISN(v_stdout,1): PUSH(Y->v_stdout);
  ISN(v_stderr,1): PUSH(Y->v_stderr);
  ISN(write,1):
    stky_io__write(Y, Vt(0,stky_io*), V(1), 9999); POPN(2);
  ISN(write_int,1):
    stky_io__printf(Y, Vt(0,stky_io*), "%lld", (long long) Vi(1)); POPN(2);
  ISN(write_char,1):
    stky_io__printf(Y, Vt(0,stky_io*), "%c", (int) stky_v_char_(V(1))); POPN(2);
  ISN(write_symbol,1): {
      stky_symbol *s = Vt(1,stky_symbol*);
      V(1) = s->name; goto I_write_string; }
  ISN(write_voidP,1):
    stky_io__printf(Y, Vt(0,stky_io*), "@%p", Vt(1,voidP)); POPN(2);
  ISN(write_string,1): {
      stky_io *io = Vt(0,stky_io*);
      PUSH(io->write_string); CALLISN(isn_eval);
    }
  ISN(write_string_FILEP,1): {
      stky_string *s = Vt(1,stky_string*); 
      fwrite(s->p, 1, s->l, Vt(0,stky_io*)->opaque); POPN(2); }
  ISN(write_string_string,1): {
      stky_string *dst = Vt(0,stky_string*); 
      stky_string *s = Vt(1,stky_string*);
      stky_string_append(Y, dst, s->p, s->l); POPN(2);
    }
  ISN(read_char,1): {
      stky_io *io = Vt(0,stky_io*);
      PUSH(io->read_char); CALLISN(isn_eval);
    }
  ISN(unread_char,1): {
      stky_io *io = Vt(0,stky_io*);
      PUSH(io->unread_char); CALLISN(isn_eval);
    }
  ISN(read_char_FILEP,1): {
      stky_io *io = Vt(0,stky_io*);
      V(0) = stky_v_char(fgetc(io->opaque));
    }
  ISN(unread_char_FILEP,1): {
      stky_io *io = V(0);
      ungetc(stky_v_char_(V(1)), io->opaque);
      POPN(2);
    }
  ISN(read_char_string,1): {
      stky_io *io = V(0);
      stky_string *s = io->opaque;
      V(0) = stky_v_char(s->l > 0 ? (s->l --, *(s->p ++)) : -1);
    }
  ISN(unread_char_string,1): {
      stky_io *io = V(0);
      stky_string *s = io->opaque;
      if ( s->p > s->b ) { s->l ++; s->p --; }
      POPN(2);
    }
  ISN(close,1): {
      stky_io *io = Vt(0,stky_io*);
      PUSH(io->close); CALLISN(isn_eval);
    }
  ISN(close_FILEP,1): {
      stky_io *io = Vt(0,stky_io*);
      if ( io->opaque ) fclose(io->opaque);
      io->opaque = 0;
      POP();
    }
  ISN(close_string,1): {
      stky_io *io = Vt(0,stky_io*);
      ((stky_string *) io->opaque)->l = 0;
      POP();
    }
  ISN(at_eos,1): {
      stky_io *io = Vt(0,stky_io*);
      PUSH(io->at_eos); CALLISN(isn_eval);
    }
  ISN(at_eos_FILEP,1): {
      stky_io *io = Vt(0,stky_io*);
      V(0) = stky_v_int(! ! feof(io->opaque));
    }
  ISN(at_eos_string,1): {
      stky_io *io = Vt(0,stky_io*);
      V(0) = stky_v_int(((stky_string *) io->opaque)->l <= 0);
    }
  ISN(c_malloc,1):  Vt(0,voidP) = stky_malloc(Vi(0));
  ISN(c_realloc,1): Vt(1,voidP) = stky_realloc(Vt(1,voidP), Vi(0)); POP();
  ISN(c_free,1):    stky_free(Vt(0,voidP)); POP();
  ISN(c_memmove,1): memmove(Vt(2,voidP), Vt(1,voidP), Vi(0)); POPN(3);
  ISN(v_tag,1):     V(0) = stky_v_int(stky_v_tag(V(0)));
  ISN(v_type,1):    V(0) = stky_v_type(V(0));
  ISN(v_flags,1):   V(0) = stky_v_int(((stky_object*) V(0))->flags);
  ISN(cve,1):       Vt(0, stky_object*)->flags |= 1;
  ISN(eval,1):      val = POP(); goto eval;
  ISN(mark,1):      PUSH(Y->v_mark);
  ISN(marke,1):     PUSH(Y->v_marke);
  ISN(ctm,1): {
      size_t n = 0; stky_v *p = vp;
      while ( p >= Y->vs.p ) {
        if ( *p == Y->v_mark || *p == Y->v_marke ) break;
        -- p;
        ++ n;
      }
      PUSH(stky_v_int(n));
    }
  ISN(array_stk,1): {
      size_t as = stky_v_int_(POP());
      stky_array *a = stky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(*a)), as);
      // fprintf(stderr, "   %lu array_stk => @%p\n", as, a);
      memmove(a->p, POPN(a->l = as), sizeof(a->p[0]) * as);
      V(0) = a;
    }
  ISN(array_new,1):
      Vt(0,stky_array*) =
        stky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(stky_array)), Vi(0));
  ISN(array_tm,1):
      stky_exec(Y, isn_ctm, isn_array_stk);
  ISN(array_tme,1):
      stky_exec(Y, isn_ctm, isn_array_stk, isn_cve);
  ISN(array_b,1):  V(0) = Vt(0,stky_array*)->b;
  ISN(array_p,1):  V(0) = Vt(0,stky_array*)->p;
  ISN(array_l,1):  V(0) = stky_v_int(Vt(0,stky_array*)->l);
  ISN(array_s,1):  V(0) = stky_v_int(Vt(0,stky_array*)->s);
  ISN(array_es,1): V(0) = stky_v_int(Vt(0,stky_array*)->es);
  ISN(array_get,1):  V(1) = Vt(0,stky_array*)->p[Vi(1)]; POP();
  ISN(array_set,1):  Vt(0,stky_array*)->p[Vi(1)] = V(2); POPN(2);
  ISN(object_dup,1):  V(0) = stky_object_dup(Y, V(0));
  ISN(bytes_dup,1):  V(0) = stky_bytes_dup(Y, V(0));
  ISN(array_push,1): // v a ARRAY_PUSH |
      stky_array_push(Y, V(0), V(1));
      POPN(2);
  ISN(array_pop,1): // a ARRAY_POP | a[-- a.l]
      V(0) = stky_array_pop(Y, V(0));
  ISN(dict_new,1): {
      stky_dict *d = stky_object_new(Y, stky_t(dict), sizeof(*d));
      stky_array_init(Y, (stky_array*) d, 8);
      d->eq = V(0);
      // fprintf(stderr, "  dict_new %lld\n", (long long) d);
      Vt(0,stky_dict*) = d;
    }
  ISN(dict_get,1): { // v k dict DICT_GET | (v|d)
      stky_dict *d = Vt(0,stky_dict*);
      stky_v k = V(1), v = V(2);
      size_t i = 0;
      while ( i + 1 < d->a.l ) {
        PUSH(k); PUSH(d->a.p[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( stky_v_int_(POP()) ) {
          v = d->a.p[i + 1];
          if ( i ) {
            swap(d->a.p[0], d->a.p[i]);
            swap(d->a.p[1], d->a.p[i + 1]);
          }
          break;
        }
        i += 2;
      }
      POPN(2);
      V(0) = v;
      }
  ISN(dict_set,1): { // v k dict DICT_SET |
      stky_dict *d = Vt(0,stky_dict*);
      stky_v k = V(1), v = V(2);
      size_t i = 0;
      while ( i + 1 < d->a.l ) {
        PUSH(k); PUSH(d->a.p[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( stky_v_int_(POP()) ) {
          d->a.p[i + 1] = v;
          if ( i ) {
            swap(d->a.p[0], d->a.p[i]);
            swap(d->a.p[1], d->a.p[i + 1]);
          }
          goto dict_set_done;
        }
        i += 2;
      }
      PUSH(k); PUSH(d); CALLISN(isn_array_push);
      PUSH(v); PUSH(d); CALLISN(isn_array_push);
    dict_set_done:
      POPN(3);
      if ( 0 && d != Y->sym_dict ) {
        fprintf(stderr, "  dict_set => "); stky_write(Y, stderr, d, 3); fprintf(stderr, "\n");
      }
      (void) 0;
    }
  ISN(sym,1): {
      stky_string *str = V(0); stky_symbol *sym;
      PUSH(0); PUSH(str); PUSH(Y->sym_dict); CALLISN(isn_dict_get);
      if ( V(0) ) {
        sym = V(1) = V(0);
        POP();
        // fprintf(stderr, "  sym @%p %s \n", sym, str->p);
      } else {
        POP();
        str = (void*) stky_bytes_dup(Y, (void*) str);
        sym = stky_object_new(Y, stky_t(symbol), sizeof(*sym));
        sym->name = str;
        PUSH(sym); PUSH(str); PUSH(Y->sym_dict); CALLISN(isn_dict_set);
        V(0) = sym;
        // fprintf(stderr, "  sym @%p %s NEW \n", sym, str->p);
      }
      // fprintf(stderr, "  sym %s", str->p); stky_print_vs(Y, stderr); fprintf(stderr, "\n");
    }
  ISN(sym_charP,1): {
      char *str = (void*) *(pc ++);
      stky_string *s = stky_string_new_charP(Y, str, -1);
      PUSH(s); CALLISN(isn_sym);
      pc[-2] = isn_lit;
      pc[-1] = (stky_i) V(0);
    }
  ISN(sym_dict,1):    PUSH(Y->sym_dict);
  ISN(dict_stack,1):  PUSH(Y->dict_stack);
  ISN(dict_stack_top,1):  PUSH(stky_array_top(Y, Y->dict_stack));
  ISN(lookup,1): {
      stky_v k = V(0);
      int i = Y->dict_stack->l;
      while ( -- i >= 0 ) {
        PUSH(Y->v_lookup_na); PUSH(k); PUSH(Y->dict_stack->p[i]); CALLISN(isn_dict_get);
        if ( V(0) != Y->v_lookup_na ) {
          V(1) = V(0); POP();
          goto lookup_done;
        }
        POP();
      }
      PUSH(Y->v_lookup_na);
      stky_catch__throw(Y, Y->error_catch, stky_array__new(Y, vp - 1, 2));
      lookup_done:
      (void) 0;
    }
  ISN(set,1): { // v k SET |
      stky_v k = V(0), v = V(1);
      int i = Y->dict_stack->l;
      while ( -- i >= 0 ) {
        PUSH(Y->v_lookup_na); PUSH(k); PUSH(Y->dict_stack->p[i]); CALLISN(isn_dict_get);
        if ( V(0) != Y->v_lookup_na ) {
          PUSH(v); PUSH(k); PUSH(Y->dict_stack->p[i]); CALLISN(isn_dict_set);
          goto set_done;
        }
        POP();
      }
      PUSH(Y->v_lookup_na);
      stky_catch__throw(Y, Y->error_catch, stky_array__new(Y, vp - 1, 2));
      set_done:
      (void) 0;
      }
  ISN(call,1):
    stky_array_push(Y, &Y->es, pc);
    val = POP();
    goto eval;
  ISN(rtn,1):
    if ( Y->es.l <= es_i ) goto rtn;
    pc = stky_array_pop(Y, &Y->es);
  ISN(Y,1): PUSH((stky_v) Y);
  ISN(c_proc,2):
#define FP Vt(0,stky_voidP*)->value
    switch ( stky_v_int_(POP()) ) {
    case 0:
      ((void (*)()) FP) (); POPN(1); break;
    case 1:
      ((void (*)(stky_v)) FP) (V(1)); POPN(2); break;
    case 2:
      ((void (*)(stky_v, stky_v)) FP) (V(2), V(1)); POPN(3); break;
    default: abort();
    }
  ISN(c_func,2):
    switch ( stky_v_int_(POP()) ) {
    case 0:
      V(0) = ((stky_v (*)()) FP) (); break;
    case 1:
      V(1) = ((stky_v (*)(stky_v)) FP) (V(1)); POPN(1); break;
    case 2:
      V(2) = ((stky_v (*)(stky_v, stky_v)) FP) (V(2), V(1)); POPN(2); break;
    case 3:
      V(3) = ((stky_v (*)(stky_v, stky_v, stky_v)) FP) (V(3), V(2), V(1)); POPN(3); break;
    case 4:
      V(4) = ((stky_v (*)(stky_v, stky_v, stky_v, stky_v)) FP) (V(4), V(3), V(2), V(1)); POPN(4); break;
    default: abort();
    }
#undef FP
  ISN(catch,1): { // body thrown CATCH
      stky_catch__BODY(c) {
        val = V(1);
        PUSH(val); CALLISN(isn_eval);
        V(2) = V(0);
        POPN(2);
      }
      stky_catch__THROWN(c) {
        SWAP(0, 1); POP();
        PUSH(c->value);
        PUSH(val); CALLISN(isn_eval);
      }
      stky_catch__END(c);
    }
  ISN(throw,1): {
      stky_catch *c = POP();
      stky_catch__throw(Y, c, POP());
    }
  ISN(error_catch,1): PUSH(Y->error_catch);
  ISN(dlopen,1):
    V(1) = stky_voidP__new(Y, dlopen(Vt(1,stky_string*)->p, stky_v_int_(V(0)))); POP();
  ISN(dlclose,1):
    dlclose(Vt(0,stky_voidP*)->value);
  ISN(dlsym,1):
    V(1) = stky_voidP__new(Y, dlsym(Vt(1,stky_voidP*)->value, Vt(0,stky_string*)->p)); POP();
  ISN(END,1): goto rtn;
  }
#undef ISN

 rtn:
  if ( Y->trace > 0 ) { fprintf(stderr, "  # }\n"); }
  return Y;
}
#undef vp
#undef swap

static
const char *char_to_str(int c)
{
  static char buf[32][8];
  static int buf_i = 0;
  char *s = buf[buf_i = (buf_i + 1) % 32];
  sprintf(s, c > 32 && isprint(c) ? "'%c'" :  "'\\0%o'", c);
  return s;
}

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

#define fgetc(IO) ({ stky_push(Y, IO); stky_exec(Y, isn_read_char); stky_v_char_(stky_pop(Y)); })
#define ungetc(C,IO) ({ stky_push(Y, stky_v_char(C)); stky_push(Y, IO); stky_exec(Y, isn_unread_char); })
stky_v stky_read_token(stky *Y, stky_io *in)
{
  stky_v value;
  stky_string *token = 0;
  stky_w n = 0;
  int pot_num_c = 0;
  int literal = 0;
  int state = s_start, last_state = -1, last_state_2;
  int c = -2;

#define take_c() c = -2
#define next_c() take_c(); goto again
#define next_s(s) state = (s); goto again
#define stop_s(s) last_state_2 = (s); next_s(s_stop)
 again:
  last_state = last_state_2; last_state_2 = state;
  if ( ! token ) token = stky_string_new_charP(Y, 0, 0);
  if ( c == -2 ) c = fgetc(in);
  if ( Y->token_debug >= 2 ) fprintf(stderr, "  c = %s, state = %d, token = '%s'\n", char_to_str(c), state, token->p);
  switch ( state ) {
  case s_error:
    stky_string_append_char(Y, token, c);
    value = token;
  case s_eos:
  case s_stop:
    break;
  case s_start:
    switch ( c ) {
    case EOF: stop_s(s_eos);
    case ' ': case '\t': case '\n': case '\r':
      next_c();
    case '#': take_c(); next_s(s_comment_eol);
    case '`':
      literal ++; next_c();
    case '-': case '+':
      stky_string_append_char(Y, token, c);
      pot_num_c = c; take_c();
      next_s(s_pot_num);
    case '0' ... '9':
      next_s(s_int);
    case '\'':
      state = s_char; next_c();
    case '"':
      state = s_string; next_c();
    case ',':
      stky_print_vs(Y, Y->v_stderr);
      next_c();
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
      stky_push(Y, token);
      stky_exec(Y, isn_sym);
      value = stky_pop(Y); next_s(s_stop);
    default:
      stky_string_append_char(Y, token, c);
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
      stky_string_append_char(Y, token, c);
      {
        stky_w prev_n = n;
        n = n * 10 + c - '0';
        if ( n < prev_n ) { take_c(); next_s(s_symbol); } // overflow
        // fprintf(stderr, "  s_int: n = %lld\n", (long long) n);
      }
      next_c();
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      if ( pot_num_c == '-' ) n = - n;
      value = stky_v_int(n); next_s(s_stop);
    default:
      next_s(s_symbol);
    }
  case s_char:
    switch ( c ) {
    default:
      value = stky_v_char(c); next_s(s_stop);
    }
  case s_string:
    switch ( c ) {
    case '"':
      value = token; take_c(); next_s(s_stop);
    default:
      stky_string_append_char(Y, token, c);
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
    value = stky_literal_new(Y, value);
    last_state = s_literal;
  }
  stky_push(Y, value);
  stky_push(Y, stky_v_int(last_state));

  if ( Y->token_debug >= 1 ) {
    stky_io__printf(Y, Y->v_stderr, "  : ");
    stky_write(Y, stderr, value, 1);
    stky_io__printf(Y, Y->v_stderr, " %d\n", last_state);
    stky_print_vs(Y, Y->v_stderr);
  }

  return Y;
}
#undef fgetc
#undef ungetc

stky *stky_io__eval(stky *Y, stky_io *io)
{
  while ( ! stky_io__at_eos(Y, io) ) {
    stky_io__eval1(Y, io);
  }
  return Y;
}

stky *stky_io__eval1(stky *Y, stky_io *in)
{
    stky_catch__BODY(c) {
      Y->error_catch = c;
      stky_read_token(Y, in);
      switch ( stky_v_int_(stky_pop(Y)) ) {
      case s_eos:
        stky_pop(Y);
        break;
      default: {
        stky_exec(Y, isn_eval);
      }
      }
    }
    stky_catch__THROWN(c) {
      PUSH(c->value);
      fprintf(stderr, "ERROR: "); stky_write(Y, stderr, stky_top(Y), 10); fprintf(stderr, "\n");
    }
    stky_catch__END(c);
    // fprintf(stderr, "  =>"); stky_print_vs(Y, stderr);
  return Y;
}

#undef fprintf
#define fprintf(O, FMT...) stky_io__printf(Y, O, ##FMT)
stky *stky_io__write(stky *Y, stky_io *out, stky_v v, int depth)
{
  int is_dict;
 again:
  if ( depth < 1 ) {
    fprintf(out, "@?"); goto rtn;
  }
  is_dict = 0;
  switch ( stky_v_type_i(v) ) {
  case stky_t_null:
    fprintf(out, "@0x0:$null"); break;
  case stky_t_voidP:
    fprintf(out, "@%p", ((stky_voidP*) v)->value); break;
  case stky_t_mark:
    fprintf(out, "%s", ((stky_string*) v)->p); break;
  case stky_t_int:
    fprintf(out, "%lld", (long long) stky_v_int_(v)); break;
  case stky_t_char:
    fprintf(out, "%s", char_to_str(stky_v_char_(v))); break;
  case stky_t_type:
    fprintf(out, "$%s", stky_v_type(v)->name); break;
  case stky_t_isn:
    fprintf(out, "%s", Y->isns[(stky_i) v].sym_name); break;
  case stky_t_string:
    fprintf(out, "\"%s\"", ((stky_string*) v)->p); break;
  case stky_t_words:
    if ( ((stky_words *) v)->name ) {
      fprintf(out, "%s:$words", ((stky_words *) v)->name); break;
    }
    goto array;
  case stky_t_dict:
    is_dict = 1;
  case stky_t_array:
  array:
    if ( depth < 2 ) goto shallow;
    {
    stky_array *a = v;
    // fprintf(out, "@%p:", v);
    if ( v == Y->sym_dict ) {
      fprintf(out, "<sym_dict>");
    } else if ( v == Y->dict_0 ) {
      fprintf(out, "<dict_0>");
    } else {
      if ( is_dict ) fprintf(out, "(");
      fprintf(out, a->o.flags & 1 ? "{ " : "[ ");
      for ( size_t i = 0; i < a->l; ++ i ) {
        stky_io__write(Y, out, a->p[i], depth - 1);
        fprintf(out, " ");
      }
      fprintf(out, a->o.flags & 1 ? "} " : "] ");
      if ( is_dict ) {
        stky_io__write(Y, out, ((stky_dict*) v)->eq, depth - 1);
        fprintf(out, " dict)");
      }
    }
  } break;
  case stky_t_symbol:
    stky_io__write_string(Y, out, ((stky_symbol*) v)->name->p, ((stky_symbol*) v)->name->l);
    break;
  case stky_t_literal:
    fprintf(out, "/"); v = ((stky_literal*) v)->value; goto again;
  default:
  shallow:
    fprintf(out, "@%p:$%s", v, stky_v_type(v)->name); break;
    abort();
  }
 rtn:
  return Y;
}
#undef fprintf

stky *stky_new(int *argcp, char ***argvp)
{
  stky *Y = 0;
  int i;

  GC_init();
  Y = stky_object_new(Y, 0, sizeof(*Y));
  Y->o.type = stky_t(stky);
  Y->trace = Y->token_debug = Y->defer_eval = 0;
  Y->argcp = argcp; Y->argvp = argvp;

  {
    stky_type *t;
    memset(Y->types, 0, sizeof(Y->types));
#define TYPE(NAME)                                              \
    t = &Y->types[stky_t_##NAME];                               \
    stky_object_init(Y, t, stky_t(type), sizeof(*t));           \
    t->i = stky_t_##NAME;                                       \
    t->name = #NAME;
#include "stacky/types.h"
  }

  Y->v_mark =  (void*) stky_string_new_charP(Y, "&&[", -1);
  Y->v_marke = (void*) stky_string_new_charP(Y, "&&{", -1);
  ((stky_object *) Y->v_mark)->type  = stky_t(mark);
  ((stky_object *) Y->v_marke)->type = stky_t(mark);

  Y->v_lookup_na = (void*) stky_string_new_charP(Y, "&&lookup_na", -1);
  ((stky_object *) Y->v_lookup_na)->type = stky_t(mark);

  stky_object_init(Y, &Y->vs, stky_t(array), sizeof(Y->vs));
  stky_array_init(Y, &Y->vs, 1023);
  Y->vs.p[Y->vs.l ++] = Y;

  stky_object_init(Y, &Y->es, stky_t(array), sizeof(Y->es));
  stky_array_init(Y, &Y->es, 1023);
  Y->es.p[Y->es.l ++] = Y;

  {
    char *v;
    if ( (v = getenv("STACKY_TRACE")) && (i = atoi(v)) > 0 )
      Y->trace = i;
  }

  for ( i = 0; isn_defs[i].name; ++ i ) {
    stky_i isn = isn_defs[i].isn;
    stky_words *isn_w;

    isn_defs[i].i = i;
    Y->isns[isn] = isn_defs[i];
    // Y->isns[isn].o.type = stky_t(isn);
    isn_w = Y->isns[isn].words = stky_words(Y, isn);
    isn_w->name = Y->isns[isn].sym_name + 1;
  }

  Y->v_stdin  = stky_io__new_FILEP(Y, stdin,  "<stdin>",  "r");
  Y->v_stdout = stky_io__new_FILEP(Y, stdout, "<stdout>", "w");
  Y->v_stderr = stky_io__new_FILEP(Y, stderr, "<stderr>", "w");

  Y->sym_dict    = stky_pop(stky_exec(Y, isn_lit, stky_isn_w(isn_string_eq), isn_dict_new));
  Y->dict_stack  = stky_pop(stky_exec(Y, isn_lit_int, 0, isn_array_new));
  Y->s_marke     = stky_pop(stky_exec(Y, isn_sym_charP, (stky_i) "{"));
  Y->s_array_tme = stky_pop(stky_exec(Y, isn_sym_charP, (stky_i) "}"));
  Y->dict_0      = stky_pop(stky_exec(Y, isn_lit, stky_isn_w(isn_eqw), isn_dict_new));
  stky_exec(Y, isn_lit, (stky_i) Y->dict_0, isn_dict_stack, isn_array_push);

  stky_exec(Y,
            isn_lit, (stky_i) stky_isn_w(isn_mark),      isn_sym_charP, (stky_i) "[", isn_dict_stack_top, isn_dict_set,
            isn_lit, (stky_i) stky_isn_w(isn_marke),     isn_sym_charP, (stky_i) "{", isn_dict_stack_top, isn_dict_set,
            isn_lit, (stky_i) stky_isn_w(isn_array_tm),  isn_sym_charP, (stky_i) "]", isn_dict_stack_top, isn_dict_set,
            isn_lit, (stky_i) stky_isn_w(isn_array_tme), isn_sym_charP, (stky_i) "}", isn_dict_stack_top, isn_dict_set);
 
  for ( i = 0; Y->types[i].name; ++ i ) {
    char name[32];
    sprintf(name, "&$%s", Y->types[i].name);
    stky_exec(Y,
              isn_lit, (stky_i) &Y->types[i], isn_sym_charP, (stky_i) name, isn_dict_stack_top, isn_dict_set);
  }

  for ( i = 0; isn_defs[i].name; ++ i ) {
    stky_i isn = isn_defs[i].isn;
    stky_v isn_w = Y->isns[isn].words;
    // fprintf(stderr, "  isn %ld => ", isn); stky_write(Y, stderr, isn_w, 2); fprintf(stderr, "\n");
    stky_exec(Y,
              isn_lit, (stky_i) isn_w,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name + 2,
              isn_dict_stack_top, isn_dict_set,

              isn_lit, (stky_i) isn_w,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name + 1,
              isn_dict_stack_top, isn_dict_set,

              isn_lit, (stky_i) isn,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name,
              isn_dict_stack_top, isn_dict_set);
  }

  Y->trace = 0;
#define BOP(N,OP) \
  stky_exec(Y, isn_sym_charP, (stky_i) ("&&" #N), isn_lookup, isn_sym_charP, (stky_i) #OP, isn_dict_stack_top, isn_dict_set);
#define UOP(N,OP) BOP(N,OP)
#include "stacky/cops.h"

  {
    int i;
    stky_array *o = stky_array__new(Y, 0, *Y->argcp);
    for ( i = 0; i < *Y->argcp; ++ i ) {
      o->p[i] = stky_string_new_charP(Y, *(Y->argvp)[i], -1);
    }
    stky_exec(Y,
              isn_lit, (stky_i) o,
              isn_sym_charP, (stky_i) "&argv",
              isn_dict_stack_top, isn_dict_set);
  }

  // Y->trace = 10;
  if ( 1 ) {
    stky_io *io = stky_io__new_FILEP(Y, 0, "boot.stky", "r");
    fprintf(stderr, "  # reading boot.stky\n");
    stky_print_vs(Y, Y->v_stderr);
    stky_io__eval(Y, io);
    stky_io__close(Y, io);
    stky_print_vs(Y, Y->v_stderr);
  }
  // Y->token_debug ++;
  // Y->trace = 10;

  return Y;
}

