enum _isn {
#define ISN(name,lits) _isn_##name,
#include "isns.h"
  _isn_END
};
enum isn {
#define ISN(name,lits) isn_##name = _stky_v_int(_isn_##name),
#include "isns.h"
  isn_END
};

