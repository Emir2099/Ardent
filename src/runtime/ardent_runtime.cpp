#include "ardent_runtime.h"
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cstdio>

extern "C" {

// -1 = no override, 0 = forced off, 1 = forced on
static int g_rt_debug_override = -1;

const char* ardent_rt_version() { return "ardent-runtime 0.0"; }

static bool rt_debug_enabled() {
    // Allow explicit override via ardent_rt_set_debug().
    static int init = 0;
    static bool enabled = false;
    if (g_rt_debug_override != -1) {
        return g_rt_debug_override != 0;
    }
    if (!init) {
        enabled = (std::getenv("ARDENT_RT_DEBUG") != nullptr);
        init = 1;
    }
    return enabled;
}

void ardent_rt_set_debug(int enabled_state) {
    // Set override and bypass environment checks.
    // Any non-zero value enables; zero disables.
    g_rt_debug_override = (enabled_state != 0) ? 1 : 0;
}

ArdentValue ardent_rt_add(ArdentValue a, ArdentValue b) {
    // Assumes both are numbers; future: type checks
    if (rt_debug_enabled()) std::fprintf(stderr, "[rt_add] a.num=%lld b.num=%lld\n", (long long)a.num, (long long)b.num);
    return ard_make_number(a.num + b.num);
}

ArdentValue ardent_rt_print(ArdentValue v) {
    if (rt_debug_enabled()) std::fprintf(stderr, "[rt_print] tag=%d\n", (int)v.tag);
    switch (v.tag) {
        case ARD_NUMBER: std::cout << v.num << std::endl; break;
        case ARD_TRUTH:  std::cout << (v.truth ? "True" : "False") << std::endl; break;
        case ARD_PHRASE: if (v.str) std::cout << v.str << std::endl; else std::cout << "" << std::endl; break;
        default: std::cout << "<unknown>" << std::endl; break;
    }
    return v;
}

ArdentValueLL ardent_rt_print_av(ArdentValueLL v) {
    if (rt_debug_enabled()) std::fprintf(stderr, "[print_av] tag=%d num=%lld truth=%d str=%p len=%d\n", (int)v.tag, (long long)v.num, (int)v.truth, (const void*)v.str, (int)v.len);
    switch (v.tag) {
        case ARD_NUMBER: std::cout << v.num << std::endl; break;
        case ARD_TRUTH:  std::cout << (v.truth ? "True" : "False") << std::endl; break;
        case ARD_PHRASE: if (v.str) std::cout << v.str << std::endl; else std::cout << "" << std::endl; break;
        default: std::cout << "<unknown>" << std::endl; break;
    }
    return v;
}

void ardent_rt_print_av_ptr(const ArdentValueLL* v) {
    if (!v) { std::cout << "<null>" << std::endl; return; }
    if (rt_debug_enabled()) std::fprintf(stderr, "[print_av_ptr] tag=%d num=%lld truth=%d str=%p len=%d\n", (int)v->tag, (long long)v->num, (int)v->truth, (const void*)v->str, (int)v->len);
    switch (v->tag) {
        case ARD_NUMBER: std::cout << v->num << std::endl; break;
        case ARD_TRUTH:  std::cout << ((v->truth != 0) ? "True" : "False") << std::endl; break;
        case ARD_PHRASE: if (v->str) std::cout << v->str << std::endl; else std::cout << "" << std::endl; break;
        default: std::cout << "<unknown>" << std::endl; break;
    }
}

void ardent_rt_concat_av_ptr(const ArdentValueLL* a, const ArdentValueLL* b, ArdentValueLL* out) {
    if (!out) return;
    if (rt_debug_enabled()) {
        std::fprintf(stderr, "[concat_av_ptr] a.tag=%d b.tag=%d | a.num=%lld b.num=%lld | a.truth=%d b.truth=%d | a.str=%p a.len=%d | b.str=%p b.len=%d\n",
            a ? (int)a->tag : -1, b ? (int)b->tag : -1,
            a ? (long long)a->num : 0LL, b ? (long long)b->num : 0LL,
            a ? (int)a->truth : -1, b ? (int)b->truth : -1,
            a ? (const void*)a->str : nullptr, a ? (int)a->len : -1,
            b ? (const void*)b->str : nullptr, b ? (int)b->len : -1);
    }
    // If both numbers: numeric addition semantics
    if (a && b && a->tag == ARD_NUMBER && b->tag == ARD_NUMBER) {
        out->tag = ARD_NUMBER;
        out->num = a->num + b->num;
        out->truth = 0;
        out->str = nullptr;
        out->len = 0;
        return;
    }
    auto stringify = [](const ArdentValueLL* v) -> std::string {
        if (!v) return std::string();
        switch (v->tag) {
            case ARD_PHRASE: return v->str ? std::string(v->str, v->len) : std::string();
            case ARD_NUMBER: return std::to_string(v->num);
            case ARD_TRUTH:  return v->truth ? std::string("True") : std::string("False");
            default: return std::string();
        }
    };
    std::string sa = stringify(a);
    std::string sb = stringify(b);

    // Apply interpreter-like phrase concatenation spacing rules:
    // Insert a single boundary space when concatenating phrase-like parts,
    // except when the right-hand side begins with punctuation. Collapse
    // double boundary spaces to a single space.
    auto beginsWithPunct = [](const std::string &s) -> bool {
        if (s.empty()) return false;
        static const std::string punct = ",.;:)]}";
        return punct.find(s.front()) != std::string::npos;
    };

    bool leftEndsSpace = !sa.empty() && sa.back() == ' ';
    bool rightStartsSpace = !sb.empty() && sb.front() == ' ';
    bool rightStartsPunct = beginsWithPunct(sb);

    if (!leftEndsSpace && !rightStartsSpace && !rightStartsPunct) {
        sa.push_back(' ');
    }
    if (!sa.empty() && !sb.empty() && sa.back() == ' ' && sb.front() == ' ') {
        sb.erase(0, 1);
    }

    std::string combined = sa + sb;
    char *buf = nullptr;
    if (!combined.empty()) {
        buf = (char*)std::malloc(combined.size() + 1);
        if (buf) {
            std::memcpy(buf, combined.c_str(), combined.size());
            buf[combined.size()] = '\0';
        }
    }
    out->tag = ARD_PHRASE;
    out->num = 0;
    out->truth = 0;
    out->str = buf;
    out->len = (int32_t)combined.size();
}

int64_t ardent_rt_add_i64(int64_t a, int64_t b) {
    if (rt_debug_enabled()) std::fprintf(stderr, "[add_i64] %lld + %lld\n", (long long)a, (long long)b);
    return a + b;
}

int64_t ardent_rt_sub_i64(int64_t a, int64_t b) {
    if (rt_debug_enabled()) std::fprintf(stderr, "[sub_i64] %lld - %lld\n", (long long)a, (long long)b);
    return a - b;
}

int64_t ardent_rt_mul_i64(int64_t a, int64_t b) {
    if (rt_debug_enabled()) std::fprintf(stderr, "[mul_i64] %lld * %lld\n", (long long)a, (long long)b);
    return a * b;
}

int64_t ardent_rt_div_i64(int64_t a, int64_t b) {
    // NOTE: Integer division, no zero-check for demo
    if (rt_debug_enabled()) std::fprintf(stderr, "[div_i64] %lld / %lld\n", (long long)a, (long long)b);
    return b == 0 ? 0 : (a / b);
}

}
