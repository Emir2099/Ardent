// ============================================================================
// ardent_runtime.cpp — Ardent 3.2 Collection Runtime Implementation
// ============================================================================
// Implementation of runtime helpers for AOT/JIT compiled Ardent code.
// These functions handle heap-allocated collections and iteration.
// ============================================================================

#include "ardent_runtime.h"
#include <vector>
#include <unordered_map>
#include <string>
#include <cstring>
#include <cstdio>
#include <cstdlib>

// ============================================================================
// Internal Structures
// ============================================================================

struct OrderImpl {
    std::vector<ArdentValue> elements;
};

struct TomeImpl {
    std::unordered_map<std::string, ArdentValue> entries;
    std::vector<std::string> keyOrder; // preserve insertion order
};

struct IterImpl {
    enum class Kind { Order, TomeKV } kind;
    size_t index = 0;
    OrderImpl* orderRef = nullptr;
    TomeImpl* tomeRef = nullptr;
};

// ============================================================================
// Order Operations
// ============================================================================

extern "C" ArdentOrder ardent_order_new(void) {
    return new OrderImpl();
}

extern "C" ArdentOrder ardent_order_from_values(int32_t count, const ArdentValue* values) {
    auto* ord = new OrderImpl();
    ord->elements.reserve(count);
    for (int32_t i = 0; i < count; i++) {
        ord->elements.push_back(values[i]);
    }
    return ord;
}

extern "C" int64_t ardent_order_len(ArdentOrder ord) {
    if (!ord) return 0;
    return static_cast<int64_t>(static_cast<OrderImpl*>(ord)->elements.size());
}

extern "C" ArdentValue ardent_order_get(ArdentOrder ord, int64_t index) {
    ArdentValue result = {};
    if (!ord) return result;
    auto* impl = static_cast<OrderImpl*>(ord);
    int64_t len = static_cast<int64_t>(impl->elements.size());
    if (index < 0) index += len;
    if (index < 0 || index >= len) return result;
    return impl->elements[static_cast<size_t>(index)];
}

extern "C" ArdentOrder ardent_order_set(ArdentOrder ord, int64_t index, ArdentValue val) {
    if (!ord) return ord;
    auto* impl = static_cast<OrderImpl*>(ord);
    int64_t len = static_cast<int64_t>(impl->elements.size());
    if (index < 0) index += len;
    if (index >= 0 && index < len) {
        impl->elements[static_cast<size_t>(index)] = val;
    }
    return ord;
}

extern "C" ArdentOrder ardent_order_push(ArdentOrder ord, ArdentValue val) {
    if (!ord) {
        ord = ardent_order_new();
    }
    auto* impl = static_cast<OrderImpl*>(ord);
    // Create new order with appended value
    auto* newOrd = new OrderImpl();
    newOrd->elements = impl->elements;
    newOrd->elements.push_back(val);
    return newOrd;
}

extern "C" ArdentOrder ardent_order_clone(ArdentOrder ord) {
    if (!ord) return ardent_order_new();
    auto* impl = static_cast<OrderImpl*>(ord);
    auto* clone = new OrderImpl();
    clone->elements = impl->elements;
    return clone;
}

extern "C" void ardent_order_free(ArdentOrder ord) {
    if (ord) delete static_cast<OrderImpl*>(ord);
}

// ============================================================================
// Tome Operations
// ============================================================================

extern "C" ArdentTome ardent_tome_new(void) {
    return new TomeImpl();
}

extern "C" ArdentTome ardent_tome_from_pairs(int32_t count, const char** keys, const ArdentValue* values) {
    auto* tome = new TomeImpl();
    for (int32_t i = 0; i < count; i++) {
        std::string key = keys[i] ? keys[i] : "";
        tome->entries[key] = values[i];
        tome->keyOrder.push_back(key);
    }
    return tome;
}

extern "C" ArdentValue ardent_tome_get(ArdentTome tome, const char* key) {
    ArdentValue result = {};
    if (!tome || !key) return result;
    auto* impl = static_cast<TomeImpl*>(tome);
    auto it = impl->entries.find(key);
    if (it != impl->entries.end()) {
        return it->second;
    }
    return result;
}

extern "C" ArdentTome ardent_tome_set(ArdentTome tome, const char* key, ArdentValue val) {
    if (!tome) tome = ardent_tome_new();
    if (!key) return tome;
    auto* impl = static_cast<TomeImpl*>(tome);
    std::string k = key;
    if (impl->entries.find(k) == impl->entries.end()) {
        impl->keyOrder.push_back(k);
    }
    impl->entries[k] = val;
    return tome;
}

extern "C" int8_t ardent_tome_has(ArdentTome tome, const char* key) {
    if (!tome || !key) return 0;
    auto* impl = static_cast<TomeImpl*>(tome);
    return impl->entries.find(key) != impl->entries.end() ? 1 : 0;
}

extern "C" ArdentOrder ardent_tome_keys(ArdentTome tome) {
    auto* ord = new OrderImpl();
    if (!tome) return ord;
    auto* impl = static_cast<TomeImpl*>(tome);
    for (const auto& k : impl->keyOrder) {
        ArdentValue v = {};
        v.tag = ARDENT_TAG_PHRASE;
        v.str = strdup(k.c_str());
        v.len = static_cast<int32_t>(k.size());
        ord->elements.push_back(v);
    }
    return ord;
}

extern "C" void ardent_tome_free(ArdentTome tome) {
    if (tome) delete static_cast<TomeImpl*>(tome);
}

// ============================================================================
// Containment
// ============================================================================

extern "C" int8_t ardent_contains(ArdentValue needle, ArdentValue haystack) {
    if (haystack.tag == ARDENT_TAG_ORDER && haystack.coll) {
        auto* ord = static_cast<OrderImpl*>(haystack.coll);
        for (const auto& elem : ord->elements) {
            if (elem.tag != needle.tag) continue;
            switch (elem.tag) {
                case ARDENT_TAG_NUMBER:
                    if (elem.num == needle.num) return 1;
                    break;
                case ARDENT_TAG_PHRASE:
                    if (elem.str && needle.str && strcmp(elem.str, needle.str) == 0) return 1;
                    break;
                case ARDENT_TAG_TRUTH:
                    if (elem.truth == needle.truth) return 1;
                    break;
                default:
                    break;
            }
        }
        return 0;
    }
    if (haystack.tag == ARDENT_TAG_TOME && haystack.coll) {
        // For tome, check key presence (needle must be phrase)
        if (needle.tag != ARDENT_TAG_PHRASE || !needle.str) return 0;
        return ardent_tome_has(haystack.coll, needle.str);
    }
    return 0;
}

// ============================================================================
// Iterator Operations
// ============================================================================

extern "C" ArdentIter ardent_iter_order(ArdentOrder ord) {
    auto* iter = new IterImpl();
    iter->kind = IterImpl::Kind::Order;
    iter->orderRef = static_cast<OrderImpl*>(ord);
    iter->index = 0;
    return iter;
}

extern "C" ArdentIter ardent_iter_tome_kv(ArdentTome tome) {
    auto* iter = new IterImpl();
    iter->kind = IterImpl::Kind::TomeKV;
    iter->tomeRef = static_cast<TomeImpl*>(tome);
    iter->index = 0;
    return iter;
}

extern "C" int8_t ardent_iter_has_next(ArdentIter iter) {
    if (!iter) return 0;
    auto* impl = static_cast<IterImpl*>(iter);
    if (impl->kind == IterImpl::Kind::Order) {
        if (!impl->orderRef) return 0;
        return impl->index < impl->orderRef->elements.size() ? 1 : 0;
    } else {
        if (!impl->tomeRef) return 0;
        return impl->index < impl->tomeRef->keyOrder.size() ? 1 : 0;
    }
}

extern "C" ArdentValue ardent_iter_next(ArdentIter iter) {
    ArdentValue result = {};
    if (!iter) return result;
    auto* impl = static_cast<IterImpl*>(iter);
    if (impl->kind == IterImpl::Kind::Order) {
        if (!impl->orderRef || impl->index >= impl->orderRef->elements.size()) return result;
        result = impl->orderRef->elements[impl->index++];
    } else {
        // For tome, just return the key as phrase
        if (!impl->tomeRef || impl->index >= impl->tomeRef->keyOrder.size()) return result;
        const std::string& key = impl->tomeRef->keyOrder[impl->index++];
        result.tag = ARDENT_TAG_PHRASE;
        result.str = strdup(key.c_str());
        result.len = static_cast<int32_t>(key.size());
    }
    return result;
}

extern "C" const char* ardent_iter_next_kv(ArdentIter iter, ArdentValue* outVal) {
    if (!iter || !outVal) return nullptr;
    auto* impl = static_cast<IterImpl*>(iter);
    if (impl->kind != IterImpl::Kind::TomeKV || !impl->tomeRef) return nullptr;
    if (impl->index >= impl->tomeRef->keyOrder.size()) return nullptr;
    const std::string& key = impl->tomeRef->keyOrder[impl->index++];
    *outVal = impl->tomeRef->entries[key];
    return key.c_str();
}

extern "C" void ardent_iter_free(ArdentIter iter) {
    if (iter) delete static_cast<IterImpl*>(iter);
}

// ============================================================================
// Value Constructors
// ============================================================================

extern "C" ArdentValue ardent_make_number(int64_t n) {
    ArdentValue v = {};
    v.tag = ARDENT_TAG_NUMBER;
    v.num = n;
    return v;
}

extern "C" ArdentValue ardent_make_phrase(const char* s, int32_t len) {
    ArdentValue v = {};
    v.tag = ARDENT_TAG_PHRASE;
    v.str = s ? strdup(s) : nullptr;
    v.len = len;
    return v;
}

extern "C" ArdentValue ardent_make_truth(int8_t b) {
    ArdentValue v = {};
    v.tag = ARDENT_TAG_TRUTH;
    v.truth = b;
    return v;
}

extern "C" ArdentValue ardent_make_order(ArdentOrder ord) {
    ArdentValue v = {};
    v.tag = ARDENT_TAG_ORDER;
    v.coll = ord;
    return v;
}

extern "C" ArdentValue ardent_make_tome(ArdentTome tome) {
    ArdentValue v = {};
    v.tag = ARDENT_TAG_TOME;
    v.coll = tome;
    return v;
}

// ============================================================================
// Value Extraction
// ============================================================================

extern "C" int64_t ardent_extract_number(ArdentValue v) {
    return (v.tag == ARDENT_TAG_NUMBER) ? v.num : 0;
}

extern "C" const char* ardent_extract_phrase(ArdentValue v) {
    return (v.tag == ARDENT_TAG_PHRASE) ? v.str : "";
}

extern "C" int8_t ardent_extract_truth(ArdentValue v) {
    return (v.tag == ARDENT_TAG_TRUTH) ? v.truth : 0;
}

extern "C" ArdentOrder ardent_extract_order(ArdentValue v) {
    return (v.tag == ARDENT_TAG_ORDER) ? v.coll : nullptr;
}

extern "C" ArdentTome ardent_extract_tome(ArdentValue v) {
    return (v.tag == ARDENT_TAG_TOME) ? v.coll : nullptr;
}

// ============================================================================
// Legacy Runtime Functions
// ============================================================================

extern "C" int64_t ardent_rt_add_i64(int64_t a, int64_t b) { return a + b; }
extern "C" int64_t ardent_rt_sub_i64(int64_t a, int64_t b) { return a - b; }
extern "C" int64_t ardent_rt_mul_i64(int64_t a, int64_t b) { return a * b; }
extern "C" int64_t ardent_rt_div_i64(int64_t a, int64_t b) { return b != 0 ? a / b : 0; }

extern "C" void ardent_rt_print_av_ptr(const ArdentValue* v) {
    if (!v) { printf("(null)\n"); return; }
    switch (v->tag) {
        case ARDENT_TAG_NUMBER:
            printf("%lld\n", (long long)v->num);
            break;
        case ARDENT_TAG_PHRASE:
            printf("%s\n", v->str ? v->str : "");
            break;
        case ARDENT_TAG_TRUTH:
            printf("%s\n", v->truth ? "True" : "False");
            break;
        case ARDENT_TAG_ORDER:
            printf("[order %p]\n", v->coll);
            break;
        case ARDENT_TAG_TOME:
            printf("{tome %p}\n", v->coll);
            break;
        default:
            printf("(unknown tag %d)\n", v->tag);
    }
}

extern "C" void ardent_rt_concat_av_ptr(const ArdentValue* a, const ArdentValue* b, ArdentValue* out) {
    if (!a || !b || !out) return;
    // Simple string concatenation
    std::string sa, sb;
    if (a->tag == ARDENT_TAG_PHRASE && a->str) sa = a->str;
    else if (a->tag == ARDENT_TAG_NUMBER) sa = std::to_string(a->num);
    else if (a->tag == ARDENT_TAG_TRUTH) sa = a->truth ? "True" : "False";
    
    if (b->tag == ARDENT_TAG_PHRASE && b->str) sb = b->str;
    else if (b->tag == ARDENT_TAG_NUMBER) sb = std::to_string(b->num);
    else if (b->tag == ARDENT_TAG_TRUTH) sb = b->truth ? "True" : "False";
    
    std::string result = sa + sb;
    out->tag = ARDENT_TAG_PHRASE;
    out->str = strdup(result.c_str());
    out->len = static_cast<int32_t>(result.size());
}
