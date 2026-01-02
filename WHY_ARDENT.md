# Why Ardent?

> *"Where code becomes poetry, and logic sings in verse."*

## The Vision

Ardent is not just another programming language. It's a manifesto that code can be **beautiful**, **readable**, and **meaningful**—without sacrificing power or performance.

## The Problem with Modern Programming

Most programming languages optimize for machines, not humans:

- **Cryptic syntax** that requires years to master
- **Terse abbreviations** that obscure intent
- **Error messages** that read like arcane incantations
- **Code** that looks the same in every language

We spend more time *reading* code than writing it. Why shouldn't that experience be pleasant?

## The Ardent Philosophy

### 1. Poetry-First Syntax

Ardent reads like verse, not machine instructions:

```ardent
Let it be known throughout the land, a phrase named hero is of "Aragorn".

By decree of the elders, a spell named greet is cast upon traveler known as name:
    Let it be proclaimed: "Hail, noble " + name + "!"

Invoke the spell greet upon hero
```

Compare to the equivalent in a traditional language:

```python
hero = "Aragorn"

def greet(name):
    print(f"Hail, noble {name}!")

greet(hero)
```

Both work. But which tells a story?

### 2. Narrative Error Messages

When things go wrong, Ardent speaks to you, not at you:

```
A curse was cast: Division by zero in spirit 'math.divide'.
The scroll was interrupted at verse 42.
```

Not:

```
Traceback (most recent call last):
  File "<stdin>", line 1, in <module>
ZeroDivisionError: division by zero
```

### 3. Strong Semantics, Beautiful Syntax

Ardent doesn't sacrifice correctness for aesthetics:

- **Static type checking** catches errors before runtime
- **Type runes** (`:whole`, `:phrase`, `:truth`) make intent explicit
- **Purity analysis** identifies side-effect-free spells
- **Deterministic evaluation** ensures predictable behavior

### 4. Native Performance

Ardent compiles to native code via LLVM:

```bash
ardent --aot scroll.ardent -o program.exe
./program
```

No interpreter overhead. No JIT warmup. Just fast, clean execution.

## Who Is Ardent For?

### Learners
Ardent's readable syntax makes it ideal for teaching programming concepts. Students grasp intent before syntax.

### Writers
Authors, poets, and storytellers who want to bring their narratives to life through interactive computation.

### Philosophers
Those who believe that code is a form of expression, not just instruction.

### Engineers
Professionals who value clarity and maintainability. Code that reads well is code that works well.

## What Ardent Is Not

- **Not a toy language**: Ardent compiles to native code and handles real-world I/O
- **Not verbose for verbosity's sake**: Every word serves a purpose
- **Not incompatible with modern tooling**: LLVM backend, package manager, IDE support

## The Journey

Ardent began as an experiment: *What if programming felt like writing?*

It evolved through:
- **1.0**: The Interpreter's Flame — Core language and REPL
- **2.0**: The Forge — LLVM compilation, native AOT
- **2.1-2.4**: Gradual typing, scrolls, streams, async foundations
- **3.0**: The Interpreter's Flame (Redux) — Language freeze, production-ready AOT

## Try It

```bash
# Install
curl -sSL https://ardent-lang.org/install.sh | sh

# Hello, World
echo 'Let it be proclaimed: "Hello, World!"' | ardent

# Compile to native
ardent --aot examples/heroes.ardent -o heroes
./heroes
```

## Join the Quest

Ardent is open source and welcomes contributors:

- **Repository**: [github.com/Emir2099/Ardent](https://github.com/Emir2099/Ardent)
- **Documentation**: [ardent-lang.org/docs](https://ardent-lang.org/docs)
- **Community**: Discord, forums, and more

---

*"Where others see syntax, we see verse.*  
*Where others run code, we recite creation."*

**Ardent — The Language of Poetic Code**
