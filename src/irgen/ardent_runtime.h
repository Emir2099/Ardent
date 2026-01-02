#pragma once
// ============================================================================
// ardent_runtime.h — Ardent 3.2 Collection Runtime Helpers for AOT/JIT
// ============================================================================
// These C-linkage functions are called by LLVM-generated native code to
// implement Order, Tome, and iteration operations that cannot be inlined.
// ============================================================================

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Opaque Handle Types
// ============================================================================
// Runtime collections are represented as opaque pointers in generated code.
// The actual implementation lives in ardent_runtime.cpp.

typedef void* ArdentOrder;   // Heap-allocated vector of ArdentValue
typedef void* ArdentTome;    // Heap-allocated map of string -> ArdentValue
typedef void* ArdentIter;    // Iterator state for Order or Tome

// ============================================================================
// ArdentValue ABI (matches compiler_ir.cpp getArdentValueTy)
// ============================================================================
// struct ArdentValue { i32 tag; i64 num; i8 truth; i8* str; i32 len; }
// Tags: 0=Number, 1=Phrase, 2=Truth, 3=Order, 4=Tome

#pragma pack(push, 1)
typedef struct ArdentValue {
    int32_t  tag;      // 0=num, 1=phrase, 2=truth, 3=order, 4=tome
    int64_t  num;      // numeric value (if tag==0)
    int8_t   truth;    // boolean value (if tag==2)
    char*    str;      // string pointer (if tag==1)
    int32_t  len;      // string length (if tag==1)
    void*    coll;     // collection pointer (if tag==3 or 4)
} ArdentValue;
#pragma pack(pop)

// Tag constants
#define ARDENT_TAG_NUMBER  0
#define ARDENT_TAG_PHRASE  1
#define ARDENT_TAG_TRUTH   2
#define ARDENT_TAG_ORDER   3
#define ARDENT_TAG_TOME    4

// ============================================================================
// Order (Array/List) Operations
// ============================================================================

// Create a new empty order
ArdentOrder ardent_order_new(void);

// Create an order from N stack values (values popped in order)
ArdentOrder ardent_order_from_values(int32_t count, const ArdentValue* values);

// Get order length
int64_t ardent_order_len(ArdentOrder ord);

// Get element at index (supports negative indexing)
ArdentValue ardent_order_get(ArdentOrder ord, int64_t index);

// Set element at index (mutates in-place, returns order)
ArdentOrder ardent_order_set(ArdentOrder ord, int64_t index, ArdentValue val);

// Append element to order (returns new order, original unchanged)
ArdentOrder ardent_order_push(ArdentOrder ord, ArdentValue val);

// Clone an order (deep copy)
ArdentOrder ardent_order_clone(ArdentOrder ord);

// Free order memory
void ardent_order_free(ArdentOrder ord);

// ============================================================================
// Tome (Map/Object) Operations
// ============================================================================

// Create a new empty tome
ArdentTome ardent_tome_new(void);

// Create a tome from N key-value pairs (keys are strings)
ArdentTome ardent_tome_from_pairs(int32_t count, const char** keys, const ArdentValue* values);

// Get value by key
ArdentValue ardent_tome_get(ArdentTome tome, const char* key);

// Set key-value pair (mutates in-place, returns tome)
ArdentTome ardent_tome_set(ArdentTome tome, const char* key, ArdentValue val);

// Check if key exists
int8_t ardent_tome_has(ArdentTome tome, const char* key);

// Get all keys as an order of phrases
ArdentOrder ardent_tome_keys(ArdentTome tome);

// Free tome memory
void ardent_tome_free(ArdentTome tome);

// ============================================================================
// Containment (abideth in)
// ============================================================================

// Check if needle is in haystack (order or tome)
// For order: checks element equality
// For tome: checks key presence
int8_t ardent_contains(ArdentValue needle, ArdentValue haystack);

// ============================================================================
// Iterator Operations
// ============================================================================

// Create iterator for order
ArdentIter ardent_iter_order(ArdentOrder ord);

// Create iterator for tome (key-value)
ArdentIter ardent_iter_tome_kv(ArdentTome tome);

// Check if iterator has more elements
int8_t ardent_iter_has_next(ArdentIter iter);

// Get next element from order iterator, advance iterator
ArdentValue ardent_iter_next(ArdentIter iter);

// Get next key-value pair from tome iterator
// Returns key, stores value in *outVal
const char* ardent_iter_next_kv(ArdentIter iter, ArdentValue* outVal);

// Free iterator
void ardent_iter_free(ArdentIter iter);

// ============================================================================
// Filtering and Transformation (higher-order helpers)
// ============================================================================
// These are called by the generated filter/transform loops.

// Note: The actual filter/transform is done in generated code with loops.
// These helpers are for the inner operations.

// ============================================================================
// Value Constructors (for AOT codegen convenience)
// ============================================================================

ArdentValue ardent_make_number(int64_t n);
ArdentValue ardent_make_phrase(const char* s, int32_t len);
ArdentValue ardent_make_truth(int8_t b);
ArdentValue ardent_make_order(ArdentOrder ord);
ArdentValue ardent_make_tome(ArdentTome tome);

// ============================================================================
// Value Extraction
// ============================================================================

int64_t ardent_extract_number(ArdentValue v);
const char* ardent_extract_phrase(ArdentValue v);
int8_t ardent_extract_truth(ArdentValue v);
ArdentOrder ardent_extract_order(ArdentValue v);
ArdentTome ardent_extract_tome(ArdentValue v);

// ============================================================================
// Existing Runtime Functions (for compatibility)
// ============================================================================

int64_t ardent_rt_add_i64(int64_t a, int64_t b);
int64_t ardent_rt_sub_i64(int64_t a, int64_t b);
int64_t ardent_rt_mul_i64(int64_t a, int64_t b);
int64_t ardent_rt_div_i64(int64_t a, int64_t b);

void ardent_rt_print_av_ptr(const ArdentValue* v);
void ardent_rt_concat_av_ptr(const ArdentValue* a, const ArdentValue* b, ArdentValue* out);

#ifdef __cplusplus
}
#endif
