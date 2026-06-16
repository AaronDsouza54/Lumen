#include "native.h"
#include "value.h"
#include "vm.h"

#include <time.h>

Value clockNative(const int argCount, Value* args) {
  if (argCount != 0) {
    runtimeError("Got %d arguments when 0 were expected", argCount);
  }

  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}
