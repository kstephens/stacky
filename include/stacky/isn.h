#ifdef ISN_DEF
#undef ISN
#else
enum _isn {
#define ISN(name,lits) _isn_##name,
#include "isns.h"
  _isn_END_
};
enum isn {
#define ISN(name,lits) isn_##name = _stky_v_isn(_isn_##name),
#include "isns.h"
  isn_END_
};
#endif
