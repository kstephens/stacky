enum isn {
#define ISN(name,lits) isn_##name,
#include "isns.h"
  isn_END
};

