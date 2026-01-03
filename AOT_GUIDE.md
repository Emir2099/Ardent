# Ardent AOT Compilation Guide

**Version:** Ardent 3.3  
**Status:** Production-Ready

This guide covers ahead-of-time (AOT) compilation of Ardent programs to native executables.

---

## Overview

Ardent 3.0 supports three execution modes:

| Mode | Command | Description |
|------|---------|-------------|
| Interpreter | `ardent --interpret file.ardent` | Direct execution, dynamic |
| JIT | `ardent --llvm file.ardent` | LLVM JIT compilation |
| **AOT** | `ardent --aot file.ardent -o prog` | Native executable |

AOT compilation is the recommended mode for production deployments.

---

## Quick Start

### Compile to Native

```bash
# Compile scroll to native executable
ardent --aot examples/heroes.ardent -o heroes.exe

# Run the native program
./heroes.exe
```

### Emit Intermediate Artifacts

```bash
# Emit LLVM IR (for inspection or custom pipeline)
ardent --emit-llvm examples/heroes.ardent
# Creates: examples/heroes.ardent.ll

# Emit object file only (for custom linking)
ardent --emit-o examples/heroes.ardent
# Creates: examples/heroes.ardent.o
```

---

## AOT Pipeline

The AOT compilation process:

```
Source (.ardent)
    │
    ▼
┌─────────────────┐
│   Parser        │ → AST
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Type Inference  │ → Typed AST
│ Type Checking   │   (enforces AOT rules)
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Optimizer       │ → Optimized AST
│ - Const fold    │   (pure spells inlined)
│ - DCE           │
│ - Purity        │
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ IR Compiler     │ → LLVM IR
│ (compiler_ir)   │
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ LLVM Backend    │ → Object file (.o)
└─────────────────┘
    │
    ▼
┌─────────────────┐
│ Linker (g++)    │ → Native executable
│ + runtime lib   │
└─────────────────┘
```

---

## AOT-Specific Requirements

### Type Annotations

In AOT mode, all variables must have known types:

```ardent
-- Interpreter mode accepts this (dynamic):
let x be 42

-- AOT mode requires explicit type:
Let it be known, a number named x :whole is of 42.
```

### Deterministic Returns

All spells must return on every control path:

```ardent
-- ERROR in AOT: missing return on else branch
By decree of the elders, a spell named abs is cast upon n known as whole:
    Should the fates decree n surpasseth 0:
        Return with n.
    -- Missing: Otherwise: Return with -n.

-- CORRECT:
By decree of the elders, a spell named abs is cast upon n known as whole:
    Should the fates decree n surpasseth 0:
        Return with n.
    Otherwise:
        Return with 0 - n.
```

### No Dynamic Features

AOT mode disallows interpreter-only features:
- Untyped variables
- Dynamic spell dispatch
- Eval-like constructs (none in Ardent yet)

---

## Runtime Linking

AOT executables link against `ardent_runtime`:

```
ardent_runtime.h   → Runtime ABI (frozen)
ardent_runtime.cpp → Runtime implementation
aot_entry_shim.cpp → Entry point wrapper
```

### Static Linking (Default)

The runtime is compiled into the executable:

```bash
ardent --aot scroll.ardent -o scroll.exe
# Single file, no dependencies
```

### Dynamic Linking (Future)

Planned for reduced executable size:

```bash
ardent --aot --shared scroll.ardent -o scroll.exe
# Requires libardent_runtime.so at runtime
```

---

## Cross-Platform Compilation

### Windows (MinGW)

```bash
ardent --aot scroll.ardent -o scroll.exe
# Uses MinGW g++ for linking
```

### Linux

```bash
ardent --aot scroll.ardent -o scroll
# Uses system g++ for linking
```

### macOS

```bash
ardent --aot scroll.ardent -o scroll
# Uses clang++ for linking
```

### Cross-Compilation

```bash
ardent --emit-o scroll.ardent --target x86_64-pc-linux-gnu
# Produces scroll.ardent.o for Linux
# Link separately with appropriate toolchain
```

---

## Debugging AOT Programs

### Source Line Mapping

AOT executables include debug info by default:

```bash
ardent --aot -g scroll.ardent -o scroll
gdb ./scroll
(gdb) break main
(gdb) run
```

### Poetic Stack Traces

When a curse (exception) occurs:

```
A curse was cast: Division by zero in spirit 'math.divide'.

The chronicle of misfortune:
  Verse 42 in spell 'calculate'
  Verse 15 in spell 'main'
  Entry point
```

---

## Performance Tips

### Enable Optimizations

```bash
# Maximum optimization
ardent --aot -O3 scroll.ardent -o scroll

# Debug build (no optimization)
ardent --aot -O0 scroll.ardent -o scroll
```

### Pure Spell Inlining

Mark spells as pure to enable inlining:

```ardent
-- This spell is automatically detected as pure
By decree of the elders, a spell named double is cast upon n known as whole:
    Return with n * 2.

-- Impure (has I/O)
By decree of the elders, a spell named report is cast upon n known as whole:
    Let it be proclaimed: n.  -- I/O makes it impure
    Return with n.
```

### Avoid Dynamic Dispatch

Use specific types instead of dynamic ones:

```ardent
-- Fast: type known at compile time
Let it be known, a number named count :whole is of 0.

-- Slower: type checked at runtime
let count be 0  -- Interpreter mode only
```

---

## Troubleshooting

### "Symbol 'ardent_entry' not found"

The entry point wasn't generated. Ensure your scroll has top-level statements.

### "Type mismatch in assignment"

AOT mode is stricter. Add type annotations:

```ardent
Let it be known, a number named x :whole is of 42.
```

### "Spell does not return on all paths"

Add `Return with` to all branches:

```ardent
Should the fates decree condition:
    Return with a.
Otherwise:
    Return with b.
```

### Linker Errors

Ensure you have g++ (MinGW on Windows) installed and in PATH.

---

## ABI Reference

The runtime ABI is frozen as of Ardent 3.0:

```c
// ArdentValue structure (16 bytes)
struct ArdentValue {
    int8_t tag;      // 0=Number, 1=Phrase, 2=Truth
    int8_t _pad[7];  // alignment
    union {
        int64_t num;
        const char* str;
        bool truth;
    };
};

// Runtime functions
extern "C" {
    ArdentValue ardent_rt_add(ArdentValue a, ArdentValue b);
    ArdentValue ardent_rt_print(ArdentValue v);
    int64_t ardent_rt_add_i64(int64_t a, int64_t b);
    // ... more in ardent_runtime.h
}
```

---

## Examples

### Hello World (AOT)

```ardent
Let it be proclaimed: "Hello, World!"
```

```bash
ardent --aot hello.ardent -o hello
./hello
# Output: Hello, World!
```

### Fibonacci (AOT)

```ardent
By decree of the elders, a spell named fib is cast upon n known as whole:
    Should the fates decree n remaineth beneath 2:
        Return with n.
    Otherwise:
        Return with Invoke the spell fib upon n - 1 + Invoke the spell fib upon n - 2.

Let it be proclaimed: Invoke the spell fib upon 30.
```

```bash
ardent --aot fib.ardent -o fib
time ./fib
# Fast execution via native code
```

---

*May your scrolls compile swiftly and your curses be ever caught.*
