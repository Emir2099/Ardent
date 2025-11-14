#pragma once
#include <cstdint>
#define ARDENT_RT_API

extern "C" {

// Stable Value ABI 
// Tag + padding + 8-byte payload used for number, phrase pointer, or boolean.
enum ArdentTag : int8_t {
    ARD_NUMBER = 0,
    ARD_PHRASE = 1,
    ARD_TRUTH  = 2
};

struct ArdentValue {
    int8_t tag;      // ArdentTag
    int8_t _pad[7];  // alignment / reserved (future: flags)
    union {
        int64_t num;      // numeric value
        const char* str;  // phrase pointer (UTF-8)
        bool truth;       // boolean (stored in low byte of num)
    };
};

// Runtime helpers (initial minimal set)
ARDENT_RT_API ArdentValue ardent_rt_add(ArdentValue a, ArdentValue b);
ARDENT_RT_API ArdentValue ardent_rt_print(ArdentValue v);
ARDENT_RT_API const char* ardent_rt_version();
// Simpler demo helper to avoid struct-by-value ABI issues in JIT
ARDENT_RT_API int64_t ardent_rt_add_i64(int64_t a, int64_t b);

// Utility constructors (inline, not exported)
static inline ArdentValue ard_make_number(int64_t v) { ArdentValue av; av.tag = ARD_NUMBER; av.num = v; return av; }
static inline ArdentValue ard_make_truth(bool b) { ArdentValue av; av.tag = ARD_TRUTH; av.truth = b; return av; }
static inline ArdentValue ard_make_phrase(const char* s) { ArdentValue av; av.tag = ARD_PHRASE; av.str = s; return av; }

}
