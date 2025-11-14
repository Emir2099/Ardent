#include "ardent_runtime.h"
#include <string>

extern "C" {
const char* ardent_rt_version() {
    return "ardent-runtime 0.0";
}
int ardent_rt_add(int a, int b) { return a + b; }
}
