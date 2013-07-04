#include "stacky/stacky.h"
#include "gc/gc.h"
#include <ctype.h>

static stky_isn isn_defs[] = {
#define ISN(name,nwords) { { 0 }, -1, isn_##name, nwords, #name, "&&" #name },
#include "stacky/isns.h"
  { 0, 0, 0, 0 },
};

#if 0
#define stky_malloc(S)    GC_malloc(S)
#define stky_realloc(P,S) GC_realloc(P,S)
#else
void *stky_malloc_(size_t s, const char *file, int line)
{
  // if ( s > 1000000 ) abort();
  void *p = GC_malloc(s);
  // fprintf(stderr, "  stky_malloc(%lu) => %p     \t %s:%d\n", (unsigned long) s, p, file, line);
  return p;
}
void *stky_realloc_(void *o, size_t s, const char *file, int line)
{
  // if ( s > 1000000 ) abort();
  void *p = GC_realloc(o, s);
  // fprintf(stderr, "  stky_realloc(%p, %lu) => %p \t %s:%d\n", o, (unsigned long) s, p, file, line);
  return p;
}
#define stky_malloc(S)    stky_malloc_(S, __FILE__, __LINE__)
#define stky_realloc(P,S) stky_realloc_(P,S, __FILE__, __LINE__)
#endif

stky_v stky_object_init(stky *y, void *p, stky_type *type, size_t size)
{
  stky_object *o = p;
  o->type = type;
  o->flags = 0;
  o->size = size;
  return o;
}

stky_v stky_object_new(stky *Y, stky_type *type, size_t size)
{
  stky_object *o = stky_malloc(size);
  stky_object_init(Y, o, type, size);
  return o;
}

stky_v stky_object_dup(stky *Y, stky_v v)
{
  stky_object *a = v;
  stky_object *b = stky_malloc(a->size);
  *b = *a;
  return b;
}

stky_v stky_literal_new(stky *Y, stky_v v)
{
  stky_literal *o = stky_object_new(Y, stky_t(literal), sizeof(*o));
  o->value = v;
  return o;
}

stky_bytes *stky_bytes_init(stky *Y, stky_bytes *a, size_t es, size_t s)
{
  size_t cs;
  a->p = a->b = stky_malloc(cs = (a->es = es) * ((a->s = s) + 1));
  memset(a->b, 0, cs);
  a->l = 0;
  return a;
}

stky_bytes *stky_bytes_dup(stky *Y, stky_bytes *a)
{
  stky_bytes *b = stky_object_dup(Y, a);
  *b = *a;
  stky_bytes_init(Y, b, a->es, a->s);
  memcpy(b->b, a->b, a->es * (a->s + 1));
  return a;
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
  return a;
}
stky_array *stky_array__new(stky *Y, stky_v *p, size_t s)
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
  stky_bytes_init(Y, (void*) o, 1, s);
  o->o.type = stky_t(string);
  if ( p ) memcpy(o->b, p, s);
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
  siglongjmp(c->jb, 1);
}

stky_v stky_top(stky *Y) { return stky_array_top(Y, &Y->vs); }
stky_v stky_top_(stky *Y, stky_v v) { return stky_array_top_(Y, &Y->vs, v); }
stky_v stky_push(stky *Y, stky_v v) { 
  if ( Y->trace > 0 ) { fprintf(stderr, "  # << "); stky_write(Y, v, stderr, 1); fprintf(stderr, "\n"); }
  return stky_array_push(Y, &Y->vs, v); }
stky_v stky_pop(stky *Y) { 
  if ( Y->trace > 0 ) { fprintf(stderr, "  # >> "); stky_write(Y, stky_top(Y), stderr, 1); fprintf(stderr, "\n"); }
  return stky_array_pop(Y, &Y->vs); }
stky_v *stky_popn(stky *Y, size_t s ) {
  if ( Y->trace > 0 ) { fprintf(stderr, "  # << # %lu\n", s); }
  return stky_array_popn(Y, &Y->vs, s); }

static
void stky_print_vs(stky *Y, FILE *out)
{
  size_t i = 0;
  fprintf(out, "  # V: ");
  while ( i < Y->vs.l ) {
    stky_write(Y, Y->vs.p[i ++], out, 2);
    fprintf(out, " ");
  }
  fprintf(out, "| \n");
}

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
    if ( 0 ) { fprintf(stderr, "  # CALLISN(%s)\n", #I); }      \
    Y->trace --;                                \
    stky_call(Y, Y->isns[I].words->p);        \
    Y->trace ++;                                \
  } while (0)

 call:
  if ( Y->trace > 0 ) { fprintf(stderr, "  # {\n"); }
#if 0
  if ( ! Y->isns[0].addr ) {
    struct isn_def *isn;
#define ISN(name,lits) isn = &Y->isns[isn_##name]; isn->addr = &&L_##name;
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
    stky_print_vs(Y, stderr);
    switch ( stky_v_type_i(*pc) ) {
    case stky_t_isn:
    if ( *pc != isn_END ) {
      fprintf(stderr, "  # I: %-20s @%p", Y->isns[*pc].name, (void*) pc);
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
      stky_write(Y, (stky_v) *pc, stderr, 1); 
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
    }
    if ( Y->defer_eval > 0 ) {
      PUSH(val); goto next_isn;
    } else {
#define TYPE(name) goto next_isn; case stky_t_##name: T_##name
      switch ( stky_v_type_i(val) ) {
      default:       PUSH(val);
      TYPE(isn):     fprintf(stderr, "\nFATAL: invalid isn %lld\n", (long long) val); abort();
      TYPE(string):  PUSH(val);
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
        // fprintf(stderr, "  # words @%p ", val); stky_write(Y, val, stderr, 2); fprintf(stderr, "\n");
        goto call_array;
      }
#undef TYPE
    }
    abort();

  case 0: goto L_rtn;
// #include "isns.c"
#define ISN(name) goto next_isn; case isn_##name: L_##name
  ISN(nul):   abort();
  ISN(hdr):
  ISN(hdr_):
  ISN(nop):
  ISN(lit):       PUSH((stky_v) *(pc ++));
  ISN(lit_int):   PUSH(stky_v_int(*(pc ++)));
  ISN(lit_charP):
    val = stky_string_new_charP(Y, (void*) *(pc ++), -1);
    pc[-2] = isn_lit; pc[-1] = (stky_i) val;
    PUSH(val);
  ISN(lit_voidP): PUSHt(*(pc ++),voidP);
  ISN(eqw): {
      V(1) = stky_v_int(V(1) == V(0));
      POP();
    }
  ISN(string_eq): {
      stky_string *a = V(1), *b = V(0);
      POP();
      V(0) = stky_v_int(a == b || (a->l == b->l && ! memcmp(a->p, b->p, a->l)));
    }
  ISN(vget):  V(0) = V(Vi(0));
  ISN(vset):  V(Vi(1)) = V(0); POPN(2);
  ISN(dup):   PUSH(V(0));
  ISN(pop):   POP();
  ISN(popn):  val = POP(); POPN(stky_v_int_(val));
  ISN(swap):  SWAP(0, 1);
  ISN(rotl): { 
      size_t n = Vi(0); POP();
      // 3 2 1 0 n | 2 1 0 3 : n = 4
      //       ^
      stky_v tmp = V(n - 1);
      memmove(&V(n - 1), &V(n - 2), sizeof(V(0)) * (n - 1));
      V(0) = tmp;
    }
#define BOP(name, op) ISN(name): V(1) = stky_v_int(Vi(1) op Vi(0)); POP();
#define UOP(name, op) ISN(name): V(0) = stky_v_int(op Vi(0));
#include "stacky/cops.h"
  ISN(ifelser):
    pc += 2;
    pc[-3] = isn_ifelse;
    pc[-2] = (stky_i) (pc + pc[-2]);
    pc[-1] = (stky_i) (pc + pc[-1]);
    pc -= 2;
    goto L_ifelse;
  ISN(ifelse):
    if ( Vi(0) ) {
      POP(); pc = ((stky_i**) pc)[0];
    } else {
      POP(); pc = ((stky_i**) pc)[1];
    }
  ISN(jmpr):
    ++ pc;
    pc[-2] = isn_jmp;
    pc[-1] = (stky_i) (pc + pc[-1]);
    pc = (stky_i*) pc[-1];
  ISN(jmp):
    pc = (stky_i*) pc[0];
  ISN(v_stdin):  PUSH(Y->v_stdin);
  ISN(v_stdout): PUSH(Y->v_stdout);
  ISN(v_stderr): PUSH(Y->v_stderr);
  ISN(write):
    stky_write(Y, V(1), Vt(0,FILE*), 9999); POPN(2);
  ISN(write_int):
    fprintf(Vt(0,FILE*), "%lld", (long long) Vi(1)); POPN(2);
  ISN(write_char):
    fprintf(Vt(0,FILE*), "%c", (int) stky_v_char_(V(1))); POPN(2);
  ISN(write_symbol): {
      stky_symbol *s = Vt(1,stky_symbolP); 
      V(1) = s->name; goto L_write_string; }
  ISN(write_string): { 
      stky_string *s = Vt(1,stky_stringP); 
      fwrite(s->b, 1, s->l, Vt(0,FILE*)); (void) POPN(2); }
  ISN(write_voidP):
    fprintf(Vt(0,FILE*), "@%p", Vt(1,voidP)); POPN(2);
  ISN(c_malloc):  Vt(0,voidP) = malloc(Vi(0));
  ISN(c_realloc): Vt(1,voidP) = realloc(Vt(1,voidP), Vi(0)); POP();
  ISN(c_free):    free(Vt(0,voidP)); POP();
  ISN(c_memmove): memmove(Vt(2,voidP), Vt(1,voidP), Vi(0)); POPN(3);
  ISN(v_tag):     V(0) = stky_v_int(stky_v_tag(V(0)));
  ISN(v_type):    V(0) = stky_v_type(V(0));
  ISN(cve):       Vt(0, stky_object*)->flags |= 1;
  ISN(eval):      val = POP(); goto eval;
  ISN(mark):      PUSH(Y->v_mark);
  ISN(marke):     PUSH(Y->v_marke); Y->defer_eval ++;
  ISN(ctm): {
      size_t n = 0; stky_v *p = vp;
      while ( p >= Y->vs.p ) {
        if ( *p == Y->v_mark || *p == Y->v_marke ) break;
        -- p;
        ++ n;
      }
      PUSH(stky_v_int(n));
    }
  ISN(array_stk): {
      size_t as = stky_v_int_(POP());
      stky_array *a = stky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(*a)), as);
      // fprintf(stderr, "   %lu array_stk => @%p\n", as, a);
      memmove(a->p, POPN(a->l = as), sizeof(a->p[0]) * as);
      V(0) = a;
    }
  ISN(array_new):
      Vt(0,stky_arrayP) =
        stky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(stky_array)), Vi(0));
  ISN(array_tm):
      stky_exec(Y, isn_ctm, isn_array_stk, isn_swap, isn_pop);
  ISN(array_tme):
      stky_exec(Y, isn_ctm, isn_array_stk, isn_swap, isn_pop, isn_cve);
  ISN(array_b):  V(0) = Vt(0,stky_arrayP)->b;
  ISN(array_p):  V(0) = Vt(0,stky_arrayP)->p;
  ISN(array_l):  V(0) = stky_v_int(Vt(0,stky_arrayP)->l);
  ISN(array_s):  V(0) = stky_v_int(Vt(0,stky_arrayP)->s);
  ISN(array_es): V(0) = stky_v_int(Vt(0,stky_arrayP)->es);
  ISN(array_get):  V(1) = Vt(1,stky_arrayP)->b[Vi(0)]; POP();
  ISN(array_set):  Vt(2,stky_arrayP)->b[Vi(1)] = V(0); POPN(2);
  ISN(array_push):
      stky_array_push(Y, V(1), V(0));
      POP();
  ISN(array_pop):
      V(0) = stky_array_pop(Y, V(0));
  ISN(dict_new): {
      stky_dict *d = stky_object_new(Y, stky_t(dict), sizeof(*d));
      stky_array_init(Y, (stky_array*) d, 8);
      d->eq = V(0);
      // fprintf(stderr, "  dict_new %lld\n", (long long) d);
      Vt(0,stky_dictP) = d;
    }
  ISN(dict_get): { // dict k d DICT_GET | (v|d)
      stky_dict *d = Vt(2,stky_dictP);
      stky_v k = V(1), v = V(0);
      size_t i = 0;
      while ( i + 1 < d->a.l ) {
        PUSH(k); PUSH(d->a.p[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( Vi(0) ) {
          POP();
          v = d->a.p[i + 1];
          break;
        }
        POP();
        i += 2;
      }
      dict_get_done:
      POPN(2);
      V(0) = v;
      }
  ISN(dict_set): { // dict k v DICT_SET |
      stky_dict *d = Vt(2,stky_dictP);
      stky_v k = V(1), v = V(0);
      size_t i = 0;
      while ( i + 1 < d->a.l ) {
        PUSH(k); PUSH(d->a.p[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( Vi(0) ) {
          POP();
          d->a.p[i + 1] = V(0);
          POPN(2);
          goto dict_set_done;
        }
        POP();
        i += 2;
      }
      POPN(2);
      PUSH(k); CALLISN(isn_array_push);
      PUSH(v); CALLISN(isn_array_push);
    dict_set_done:
      if ( 0 && d != Y->sym_dict ) {
        fprintf(stderr, "  dict_set => "); stky_write(Y, d, stderr, 3); fprintf(stderr, "\n");
      }
      (void) 0;
    }
  ISN(sym): {
      stky_string *str = V(0); stky_symbol *sym;
      PUSH(Y->sym_dict); PUSH(str); PUSH(0); CALLISN(isn_dict_get);
      if ( V(0) ) {
        sym = V(0);
        fprintf(stderr, "  sym @%p %s \n", sym, str->p);
      } else {
        POP();
        str = (void*) stky_bytes_dup(Y, (void*) str);
        sym = stky_object_new(Y, stky_t(symbol), sizeof(*sym));
        sym->name = str;
        PUSH(Y->sym_dict); PUSH(str); PUSH(sym); CALLISN(isn_dict_set);
        fprintf(stderr, "  sym @%p %s NEW \n", sym, str->p);
      }
      POP();
      V(0) = sym;
      // fprintf(stderr, "  sym %s", str->p); stky_print_vs(Y, stderr); fprintf(stderr, "\n");
    }
  ISN(sym_charP): {
      char *str = (void*) *(pc ++);
      stky_string *s = stky_string_new_charP(Y, str, -1);
      PUSH(s); CALLISN(isn_sym);
      pc[-2] = isn_lit;
      pc[-1] = (stky_i) V(0);
    }
  ISN(sym_dict):   PUSH(Y->sym_dict);
  ISN(dict_stack): PUSH(Y->dict_stack);
  ISN(dict_stack_top):
      PUSH(stky_array_top(Y, Y->dict_stack));
  ISN(lookup): {
      stky_v k = V(0);
      int i = Y->dict_stack->l;
      while ( -- i >= 0 ) {
        PUSH(Y->dict_stack->p[i]); PUSH(k); PUSH(Y->v_lookup_na); CALLISN(isn_dict_get);
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
  ISN(call):
    stky_array_push(Y, &Y->es, pc);
    val = POP();
    goto eval;
  ISN(rtn):
    if ( Y->es.l <= es_i ) goto rtn;
    pc = stky_array_pop(Y, &Y->es);
  ISN(Y): PUSH((stky_v) Y);
  ISN(c_proc):
    switch ( *(pc ++) ) {
    case 0:
      ((void (*)()) V(0)) (); POPN(1); break;
    case 1:
      ((void (*)(stky_v)) V(0)) (V(1)); POPN(2); break;
    case 2:
      ((void (*)(stky_v, stky_v)) V(0)) (V(2), V(1)); POPN(3); break;
    default: abort();
    }
  ISN(c_func):
    switch ( *(pc ++) ) {
    case 0:
      V(0) = ((stky_v (*)()) V(0)) (); break;
    case 1:
      V(1) = ((stky_v (*)(stky_v)) V(0)) (V(1)); POPN(1); break;
    case 2:
      V(2) = ((stky_v (*)(stky_v, stky_v)) V(0)) (V(2), V(1)); POPN(2); break;
    default: abort();
    }
  ISN(catch): { // body thrown CATCH
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
  ISN(throw): {
      stky_catch *c = POP();
      stky_catch__throw(Y, c, POP());
    }
  ISN(END): goto rtn;
  }
#undef ISN

 rtn:
  if ( Y->trace > 0 ) { fprintf(stderr, "  # }\n"); }
  return Y;
}
#undef vp

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

stky_v stky_read_token(stky *Y, FILE *in)
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
  if ( Y->token_debug ) fprintf(stderr, "  c = %s, state = %d, token = '%s'\n", char_to_str(c), state, token->p);
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
    case '/':
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
      fprintf(stderr, "\n");
      stky_print_vs(Y, stderr);
      fprintf(stderr, "\n");
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

  if ( 0 ) {
    fprintf(stderr, "  : ");
    stky_write(Y, value, stderr, 1);
    fprintf(stderr, " %d\n", last_state);
    stky_print_vs(Y, stderr);
  }

  return Y;
}

stky *stky_repl(stky *Y, FILE *in, FILE *out)
{
  int c = 0;
  while ( ! feof(in) ) {
    stky_catch__BODY(c) {
      Y->error_catch = c;
      stky_read_token(Y, in);
      switch ( stky_v_int_(stky_pop(Y)) ) {
      case s_eos:
        break;
      default: {
        stky_exec(Y, isn_eval);
      }
      }
    }
    stky_catch__THROWN(c) {
      PUSH(c->value);
      fprintf(stderr, "ERROR: "); stky_write(Y, stky_top(Y), stderr, 2); fprintf(stderr, "\n");
    }
    stky_catch__END(c);
    fprintf(stderr, "  =>");
    stky_print_vs(Y, stderr);
  }
  return Y;
}

stky *stky_write(stky *Y, stky_v v, FILE *out, int depth)
{
  int is_dict;
 again:
  if ( depth < 1 ) {
    fprintf(out, "@?"); goto rtn;
  }
  is_dict = 0;
  switch ( stky_v_type_i(v) ) {
  case stky_t_null:
    fprintf(out, "@x0:$null"); break;
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
    fprintf(out, "@%p:", v);
    if ( v == Y->sym_dict ) {
      fprintf(out, "<sym_dict>");
    } else if ( v == Y->dict_0 ) {
      fprintf(out, "<dict_0>");
    } else {
      if ( is_dict ) fprintf(out, "(");
      fprintf(out, a->o.flags & 1 ? "{ " : "[ ");
      for ( size_t i = 0; i < a->l; ++ i ) {
        stky_write(Y, a->p[i], out, depth - 1);
        fprintf(out, " ");
      }
      fprintf(out, a->o.flags & 1 ? "} " : "] ");
      if ( is_dict ) {
        stky_write(Y, ((stky_dict*) v)->eq, out, depth - 1);
        fprintf(out, " dict)");
      }
    }
  } break;
  case stky_t_symbol:
    fwrite(((stky_symbol*) v)->name->p, ((stky_symbol*) v)->name->l, 1, out);
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

stky *stky_new()
{
  stky *Y = 0;
  int i;

  GC_init();
  Y = stky_object_new(Y, stky_t(stky), sizeof(*Y));
  Y->o.type = stky_t(stky);
  Y->v_stdin  = stdin;
  Y->v_stdout = stdout;
  Y->v_stderr = stderr;

  Y->v_mark =  (void*) stky_string_new_charP(Y, "[", 1);
  Y->v_marke = (void*) stky_string_new_charP(Y, "{", 1);
  ((stky_object *) Y->v_mark)->type  = stky_t(mark);
  ((stky_object *) Y->v_marke)->type = stky_t(mark);

  Y->v_lookup_na = (void*) stky_string_new_charP(Y, "&&lookup_na", -1);
  ((stky_object *) Y->v_lookup_na)->type = stky_t(mark);

  {
    stky_type *t;
#define TYPE(NAME)                                              \
    t = &Y->types[stky_t_##NAME];                               \
    stky_object_init(Y, t, stky_t(type), sizeof(*t));           \
    t->i = stky_t_##NAME;                                       \
    t->name = #NAME;
#include "stacky/types.h"
  }

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

  Y->sym_dict = stky_pop(stky_exec(Y, isn_lit, stky_isn_w(isn_string_eq), isn_dict_new));
  Y->dict_stack = stky_pop(stky_exec(Y, isn_lit_int, 0, isn_array_new));
  Y->s_marke     = stky_pop(stky_exec(Y, isn_sym_charP, (stky_i) "{"));
  Y->s_array_tme = stky_pop(stky_exec(Y, isn_sym_charP, (stky_i) "}"));
  Y->dict_0      = stky_pop(stky_exec(Y, isn_lit, stky_isn_w(isn_eqw), isn_dict_new));
  stky_exec(Y, isn_dict_stack, isn_lit, (stky_i) Y->dict_0, isn_array_push, isn_pop);

  for ( i = 0; isn_defs[i].name; ++ i ) {
    stky_i isn = isn_defs[i].isn;
    stky_v isn_w = Y->isns[isn].words;
    // fprintf(stderr, "  isn %ld => ", isn); stky_write(Y, isn_w, stderr, 2); fprintf(stderr, "\n");
    stky_exec(Y,
              isn_dict_stack_top,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name + 2,
              isn_lit, (stky_i) isn_w,
              isn_dict_set, isn_pop,
              isn_dict_stack_top,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name + 1,
              isn_lit, (stky_i) isn_w,
              isn_dict_set, isn_pop,
              isn_dict_stack_top,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name,
              isn_lit, (stky_i) isn,
              isn_dict_set, isn_pop);
  }

#define BOP(N,OP) stky_exec(Y, isn_dict_stack_top, isn_sym_charP, (stky_i) #OP, isn_sym_charP, (stky_i) ("&&" #N), isn_lookup, isn_dict_set, isn_pop);
#define UOP(N,OP) BOP(N,OP)
#include "stacky/cops.h"

  stky_exec(Y,
            isn_dict_stack_top, isn_sym_charP, (stky_i) "[", isn_lit, stky_isn_w(isn_mark),  isn_dict_set, isn_pop,
            isn_dict_stack_top, isn_sym_charP, (stky_i) "{", isn_lit, stky_isn_w(isn_marke), isn_dict_set, isn_pop,
            isn_dict_stack_top, isn_sym_charP, (stky_i) "]", isn_lit, stky_isn_w(isn_array_tm),  isn_dict_set, isn_pop,
            isn_dict_stack_top, isn_lit, (stky_i) Y->s_array_tme, isn_lit, stky_isn_w(isn_array_tme), isn_dict_set, isn_pop);

  fprintf(stderr, "\n\n dict_stack:\n");
  stky_write(Y, Y->dict_stack, stderr, 9999);
  fprintf(stderr, "\n\n");

  if ( 0 ) {
    FILE *fp;

    fprintf(stderr, "  # reading boot.stky\n");
    if ( (fp = fopen("boot.stky", "r")) ) {
      stky_repl(Y, fp, 0);
      fclose(fp);
    } else {
      perror("cannot read boot.stky");
    }

    fprintf(stderr, "\n\n dict_stack:\n");
    stky_write(Y, Y->dict_stack, stderr, 9999);
    fprintf(stderr, "\n\n");
  }
  // Y->token_debug ++;
  Y->trace = 10;

  return Y;
}

