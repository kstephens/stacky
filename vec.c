#ifndef T__
#define T__(T,N) T##_##N
#define T_(T,N) T_(T,N)
#define s_T_(T,N)s_##T##N
#define s_T(T,N)s_T_(T,N)
#define s_FT_(T,N)s_F(T##_##N)
#define s_FT(T,N)s_FT_(T,N)
#endif

void s_T(T,_resize)(Y__, s_T(T,)* self, size_t s)
{
  size_t ms = sizeof(self->b[0]) * (s + 1);
  T_t* b = self->b ?
    GC_realloc(self->b, ms) :
    GC_malloc(ms);
  self->p = b + (self->p - self->b);
  *(self->e = (self->b = b) + (self->s = s)) = 0;
}
void s_T(T,_init)(Y__, s_T(T,)* self, size_t s)
{
  self->b = self->p = self->e = 0;
  self->s = 0;
  s_T(T,_resize)(Y, self, s);
}
void s_T(T,_push)(Y__, s_T(T,)* self, s_v v)
{
  if ( self->p >= self->e )
    s_T(T,_resize)(Y, self, self->s * 3 / 2 + 3);
  *(self->p ++) = T_unbox(v);
}
s_v s_T(T,_pop)(Y__, s_T(T,)* self)
{
  return self->p > self->b ? T_box(*(-- self->p)) : 0;
}

s_FT(T,make) // v1 v2 ... vn n | [ v1 v2 ... vn ]
{
  size_t s = s_v_i_(s_pop());
  s_T(T,)* self = s_type_alloc(Y, s_t(T));
  s_T(T,_init)(Y, self, s);
  for ( size_t i = 0; i < s; ++ i )
    self->b[i] = T_unbox(V(s - 1 - i));
  self->p = self->b + s;
  s_popn(s);
  s_push(s_v_o(self));
}
s_FT(T,dup) { /// array | array-copy
  s_T(T,)* this = s_O(V(0), T);
  size_t s = this->p - this->b;
  s_T(T,)* self = s_type_alloc(Y, s_t(T));
  s_T(T,_init)(Y, self, s);
  memcpy(self->b, this->b, sizeof(self->b[0]) * s);
  self->p = self->b + s;
  V(0) = s_v_o(self);
}
s_FT(T,push) { // array v |
  s_v v = s_pop();
  s_T(T,)* self = s_O(s_pop(), T);
  s_T(T,_push)(Y, self, v);
}
s_FT(T,pop) { // array | v || null
  s_T(T,)* self = s_O(V(0), T);
  V(0) = self->p > self->b ? T_box(*(-- self->p)) : 0;
}
s_FT(T,len) { // [ e0 ... en ] | n - 1
  s_T(T,)* self = s_O(V(0), T);
  V(0) = s_v_i(self->p - self->b);
}
s_FT(T,size) { // [ e0 ... en ] | n - 1
  s_T(T,)* self = s_O(V(0), T);
  V(0) = s_v_i(self->s);
}
s_FT(T,top) { // [ e0 ... en ] | en
  V(0) = T_box(s_O(V(0), T)->p[-1]);
}
s_FT(T,get) { // i [ e0 ... ] | ei
  s_T(T,)* self = s_O(s_pop(), T);
  s_i i = s_v_i_(s_pop());
  s_push(T_box(self->b[i]));
}
s_FT(T,set) { // v i [ e0 ... ] |
  s_T(T,)* self = s_O(s_pop(), T);
  s_i i = s_v_i_(s_pop());
  s_v v = s_pop();
  self->b[i] = T_unbox(v);
}
s_FT(T,each) { // [ e0 .. en ] f | e0 f .. en f
  s_v f = s_pop();
  s_T(T,)* self = s_O(s_pop(), T);
  T_t* p = self->b;
  if ( p < self->p ) {
    for ( size_t i = 0; (p = self->b + i) + 1 < self->p; ++ i ) {
      s_push(T_box(*p));
      s_eval(Y, f);
    }
    s_push(T_box(*p));
    s_eval(Y, f); // TAILCALL
  }
}

#undef T
#undef T_t
#undef T_box
#undef T_unbox
