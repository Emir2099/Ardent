#include "ardent_runtime.h"
#include <iostream>

extern "C" {

const char* ardent_rt_version() { return "ardent-runtime 0.0"; }

ArdentValue ardent_rt_add(ArdentValue a, ArdentValue b) {
    // Assumes both are numbers; future: type checks
    return ard_make_number(a.num + b.num);
}

ArdentValue ardent_rt_print(ArdentValue v) {
    switch (v.tag) {
        case ARD_NUMBER: std::cout << v.num << std::endl; break;
        case ARD_TRUTH:  std::cout << (v.truth ? "True" : "False") << std::endl; break;
        case ARD_PHRASE: if (v.str) std::cout << v.str << std::endl; else std::cout << "" << std::endl; break;
        default: std::cout << "<unknown>" << std::endl; break;
    }
    return v;
}

int64_t ardent_rt_add_i64(int64_t a, int64_t b) {
    return a + b;
}

}
