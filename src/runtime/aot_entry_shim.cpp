extern "C" int ardent_entry();
#include "ardent_runtime.h"
extern "C" int main() {
#ifdef NDEBUG
    // Silence runtime debug logs in AOT Release executables
    ardent_rt_set_debug(0);
#endif
    return ardent_entry();
}
