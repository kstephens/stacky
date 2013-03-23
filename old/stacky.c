#include <stdlib.h>
#include <stdio.h>
#include <string.h>

struct value;
typedef struct value value;
struct value {
  value *t;
  union { 
    ssize_t i;
    double  f;
    void   *p;
    struct value_s { 
      char  *p;
      size_t l;
    } *s;
    struct value_a { 
      value *p;
      size_t l;
    } *a;
  } u;
};

typedef struct stacky {
  value t_t, t_i, t_p, t_f, t_s, t_l, t_w, t_a, t_d, t_file;
  value v_s[256], d_s[256];
  int v_i, d_i;
  value v_stdin, v_stdout, v_stderr;
  value word_dict, w_EQ;
} stacky;
static char s_EOF[] = "EOF";
static char s_mark[] = "mark";

#define V (Y->v_s + Y->v_i)

void stacky_push(stacky *Y, value v)
{
  ++ Y->v_i;
  *V = v;
}
#define PUSH(X) stacky_push(Y, (X))

value stacky_pop(stacky *Y)
{
  value v = *V;
  -- Y->v_i;
  return v;
}
#define POP(X) stacky_pop(Y)

void stacky_swap(stacky *Y)
{
  value v = V[0];
  V[0] = V[-1];
  V[-1] = v;
}
#define SWAP() stacky_swap(Y)

void stacky__dump_stack(stacky *Y)
{
  FILE *fp = stderr;
  size_t i;
  for ( i = 0; i < Y->v_i; ++ i ) {
    value v = Y->v_s[i];
    fprintf(fp, " %p:", v.t);
    switch ( v.t->u.i ) {
    case 0: fprintf(fp, "%s ", (char*) v.u.p); break;
    case 1: fprintf(fp, "%lld ", (long long) v.u.i); break;
    case 2: fprintf(fp, "%g ", (double) v.u.f); break;
    case 3: fprintf(fp, "\"%s\" ", (char*) v.u.s->p); break;
    case 4: fprintf(fp, "@%s ", (char*) v.u.s->p); break;
    default:
      fprintf(fp, "%p ", (void*) v.u.p); break;
    }
  }
}
void stacky__write_stack(stacky *Y, value out);

void stacky__int(stacky *Y, ssize_t i)
{
  value v;
  v.t = &Y->t_i;
  v.u.i = i;
  PUSH(v);
}

void stacky__str(stacky *Y, const char *p, size_t l)
{
  value v;
  v.t = &Y->t_s;
  v.u.s = (void*) malloc(sizeof(*v.u.s));
  v.u.s->p = malloc(sizeof(p[0]) * (l + 1));
  v.u.s->l = l;
  if ( p ) memcpy(v.u.s->p, p, sizeof(*p) * l);
  v.u.s->p[l] = 0;
  PUSH(v);
}

void stacky_EQ(stacky *Y)
{
  if ( V[-1].t == V[0].t ) {
    switch ( V[-1].t->u.i ) {
    case 1:
      V[-1].u.i = V[-1].u.i == V[0].u.i; break;
    case 2:
      V[-1].u.i = V[-1].u.f == V[0].u.f; break;
    case 3:
      V[-1].u.i = (V[-1].u.s->l == V[0].u.s->l && ! memcmp(V[-1].u.s->p, V[0].u.s->p, V[0].u.s->l));
      break;
    default:
      V[-1].u.i = V[-1].u.p == V[0].u.p;
      break;
    }
  }
  V[-1].t = &Y->t_i;
  -- Y->v_i;
}

void stacky__arr(stacky *Y, const value *p, size_t l)
{
  value v;
  v.t = &Y->t_s;
  v.u.a = malloc(sizeof(*v.u.a));
  v.u.a->p = malloc(sizeof(p[0]) * (l + 1));
  v.u.a->l = l;
  if ( p ) memcpy(v.u.a->p, p, sizeof(p[0]) * l);
  PUSH(v);
}

void stacky_arra(stacky *Y) // ARR VALUE | ARR
{
  size_t l = V[-1].u.a->l + 1;
  V[-1].u.a->p = realloc(V[-1].u.a->p, sizeof(V[-1].u.a->p[0]) * l);
  V[-1].u.a->p[l - 1] = V[0];
  V[-1].u.a->l = l;
  POP();
}

void stacky_dict(stacky *Y)
{
  V->t = &Y->t_d;
}

void stacky_dget(stacky *Y) // DICT KEY DEFAULT | [ VALUE | DEFAULT ]
{
  value d = V[-2], k = V[-1], v = V[0];
  size_t i = 0;
  while ( i < d.u.a->l ) {
    stacky__dump_stack(Y);
    PUSH(d.u.a->p[i]); PUSH(k); stacky_EQ(Y);
    if ( V->u.i ) {
      POP();
      V[-2] = d.u.a->p[i + 1];
      Y->v_i -= 2;
      return;
    }
    POP();
    i += 2;
  }
  V[-2] = v;
  Y->v_i -= 2;
}

void stacky_dset(stacky *Y) // DICT KEY VALUE | 
{
  value d = V[-2], k = V[-1], v = V[0];
  size_t i = 0;
  while ( i < d.u.a->l ) {
    PUSH(v); PUSH(d.u.a->p[i]); stacky_EQ(Y);
    if ( V->u.i ) {
      POP();
      d.u.a->p[i + 1] = v;
      Y->v_i -= 3;
      return;
    }
    POP();
    i += 2;
  }
  
  Y->v_i -= 3;
  PUSH(d);
  PUSH(k); stacky_arra(Y);
  PUSH(v); stacky_arra(Y);
  POP();
}

void stacky_word(stacky *Y) // STR | WORD
{
  value str, word;
  str = V[0];
  PUSH(Y->word_dict); PUSH(str); stacky__int(Y, 0);
  stacky__dump_stack(Y);
  stacky_dget(Y);
  if ( ! V->u.i ) {
    POP();
    stacky__str(Y, str.u.s->p, str.u.s->l);
    str = V[0];
    word = str; word.t = &Y->t_w;
    PUSH(Y->word_dict); PUSH(str); PUSH(word);
    // stacky__dump_stack(Y);
    stacky_dset(Y);
    POP();
    POP();
    PUSH(word);
    // stacky__dump_stack(Y);
    fprintf(stderr, "\n # new word: %s\n", V->u.s->p);
  } else {
    SWAP();
    POP();
  }
}

void stacky__word(stacky *Y, const char *p)
{
  struct value_s s = { (char*) p, strlen(p) };
  value v; v.t = &Y->t_s; v.u.s = &s;
  PUSH(v);
  stacky_word(Y);
}

void stacky_mark(stacky *Y)
{
  value v;
  v.t = &Y->t_t;
  v.u.p = s_mark;
  PUSH(v);
}

void stacky_read(stacky *Y)
{
  int c;
  value file = *V;
#define fp ((FILE*) file.u.p)
 read: c = fgetc(fp);
 parse: switch ( c ) {
  case EOF:
    V->t = &Y->t_t; V->u.p = s_EOF;
    break;
  case ' ': case '\t': case '\n': case '\r':
    goto read;
    break;
  case '#':
    while ( (c = fgetc(fp)) != EOF && c != '\n' ) ;
    goto read;
  case '0' ... '9':
    V->t = &Y->t_i; V->u.i = c - '0';
    while ( isdigit(c = fgetc(fp)) ) {
      V->u.i *= 10; V->u.i += c - '0';
    }
    if ( c == '.' ) {
      V->t = &Y->t_f; V->u.f = V->u.i;
      // FIXME
    }
    break;
  case '"':
    V->t = &Y->t_s;
    V->u.s = (void*) malloc(sizeof(V->u.s[0]));
    V->u.s->p = malloc(1); V->u.s->l = 0;
    while ( (c = fgetc(fp)) != '"' ) {
      (V->u.s->p = realloc(V->u.s->p, V->u.s->l + 1))[V->u.s->l] = c;
      ++ V->u.s->l;
    }
    V->u.s->p[V->u.s->l] = 0;
    break;
  case '@':
    stacky_read(Y);
    V->t = &Y->t_l;
    break;
  default:
    V->t = &Y->t_w;
    V->u.s = malloc(sizeof(V->u.s[0]));
    V->u.s->p = malloc(1); V->u.s->l = 0;
    while ( c != EOF && ! isspace(c) ) {
      (V->u.s->p = realloc(V->u.s->p, V->u.s->l + 1))[V->u.s->l] = c;
      ++ V->u.s->l;
      c = fgetc(fp);
    }
    V->u.s->p[V->u.s->l] = 0;
    stacky_word(Y);
  }
#undef fp
}

stacky *stacky_init()
{
  stacky *Y = malloc(sizeof(*Y));
  memset(Y, 0, sizeof(*Y));
  Y->v_i = Y->d_i = -1;
  Y->t_t.u.i = 0;
  Y->t_i.u.i = 1;
  Y->t_f.u.i = 2;
  Y->t_s.u.i = 3;
  Y->t_l.u.i = 4;
  Y->t_w.u.i = 5;
  Y->t_a.u.i = 6;
  Y->t_d.u.i = 7;
  Y->t_file.u.i = 8;
  Y->v_stdin.t = &Y->t_file;  Y->v_stdin.u.p = stdin;
  Y->v_stdout.t = &Y->t_file; Y->v_stdout.u.p = stdout;
  Y->v_stderr.t = &Y->t_file; Y->v_stderr.u.p = stderr;
  stacky_mark(Y);
  stacky__arr(Y, 0, 0);
  stacky_dict(Y);
  stacky__dump_stack(Y);
  Y->word_dict = POP();
  stacky__word(Y, "="); Y->w_EQ = POP();
  return Y;
}

void stacky_write(stacky *Y) // FILE V w | FILE
{
  value file = V[-1];
#define fp ((FILE*) file.u.p)
 write:
  switch ( V->t->u.i ) {
  case 0: fprintf(fp, "%s ", (char*) V->u.p); break;
  case 1: fprintf(fp, "%lld ", (long long) V->u.i); break;
  case 2: fprintf(fp, "%g ", (double) V->u.f); break;
  case 3: fprintf(fp, "\"%s\" ", (char*) V->u.s->p); break;
  case 4: 
    fprintf(fp, "@");
    V->t = &Y->t_w;
    goto write;
  case 5: fprintf(fp, "%s ", (char*) V->u.s->p); break;
  case 6:
    fprintf(fp, "[ ");
    {
      size_t i; value v = *V;
      for ( i = 0; i < v.u.a->l; ++ i ) {
        PUSH(file);
        PUSH(v.u.a->p[i]);
        stacky_write(Y);
        POP();
      }
    }
    fprintf(fp, "] ");
    break;
  case 7:
    fprintf(fp, "#<dict ");
    V->t = &Y->t_a; stacky_write(Y);
    fprintf(fp, "># ");
    break;
  default: 
    fprintf(fp, "#[%p %p] ", (void*) V->t, (void*) V->u.p);
    break;
  }
#undef fp
  -- Y->v_i;
}

void stacky__write_stack(stacky *Y, value out)
{
  int v_top, i;
  v_top = Y->v_i;
  PUSH(out);
  for ( i = 0; i <= v_top; ++ i ) {
    PUSH(Y->v_s[i]); stacky_write(Y);
  }
  POP();
  fprintf(out.u.p, "|\n");
}

void stacky_eval(stacky *Y)
{
 eval:
  if ( V->t->u.i <= 4 )
    return;
  if ( V->t == &Y->t_w ) {
    switch ( V->u.s->l ) {
    case 1:
      switch ( V->u.s->p[0] ) {
      case '+': V[-2].u.i = V[-2].u.i + V[-1].u.i; Y->v_i -= 2; break;
      case '-': V[-2].u.i = V[-2].u.i - V[-1].u.i; Y->v_i -= 2; break;
      case '*': V[-2].u.i = V[-2].u.i * V[-1].u.i; Y->v_i -= 2; break;
      case '/': V[-2].u.i = V[-2].u.i / V[-1].u.i; Y->v_i -= 2; break;
      case '%': V[-2].u.i = V[-2].u.i % V[-1].u.i; Y->v_i -= 2; break;
      case '=':
        POP(); stacky_EQ(Y);
        break;
      case '?': // T F Q ? | [T | F] eval
        if ( V[-1].u.i ) {
          Y->v_i -= 3;
        } else {
          Y->v_i -= 3;
          V[0] = V[1];
        }
        goto eval;
      case 'p':
        *V = Y->v_stdout; SWAP();
        stacky_write(Y); POP();
        fprintf(stdout, "\n");
        break;
      case 'r': 
        *V = Y->v_stdin; stacky_read(Y);
        break;
      }
      break;
    case 2:
      break;
    case 3:
      if ( ! strcmp(V->u.s->p, "pop") ) Y->v_i -= 2;
      else if ( ! strcmp(V->u.s->p, "nup") ) { V[-1] = V[- (V[-1].u.i + 1)]; -- Y->v_i; }
      break;
    case 4:
      if ( ! strcmp(V->u.s->p, "push") ) *V = V[-1]; 
      else if ( ! strcmp(V->u.s->p, "swap") ) { -- Y->v_i; SWAP(); }
      else if ( ! strcmp(V->u.s->p, "dget") ) { -- Y->v_i; stacky_dget(Y); }
      break;
    }
  }
}

void stacky_repl(stacky *Y) // OUT IN repl
{
  value out = V[-1];
  value in = V[0];
  int v_top, i;
 repl:
  stacky__write_stack(Y, out);
  fprintf(out.u.p, "# ");
  PUSH(in); stacky_read(Y);
  if ( ! (V->t == &Y->t_t && V->u.p == s_EOF) ) {
    stacky_eval(Y);
    goto repl;
  }
  Y->v_i -= 2;
}

int main(int argc, char **argv)
{
  stacky *Y = stacky_init();
  PUSH(Y->v_stderr); PUSH(Y->v_stdin); stacky_repl(Y);
}

