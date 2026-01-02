# Ardent 3.2 Language Specification

**Status:** FROZEN  
**Version:** 3.2.0  
**Date:** 2026-01-02  

> *"Where code becomes poetry, and logic sings in verse."*

This document defines the complete syntax and semantics of the Ardent programming language as of version 3.2. All implementations MUST conform to this specification. No breaking changes will be introduced within the 3.x series.

---

## 1. Lexical Structure

### 1.1 Encoding
All Ardent source files MUST be encoded in UTF-8. The lexer processes input as a sequence of Unicode scalar values.

### 1.2 Tokens

#### 1.2.1 Literals

| Type | Pattern | Examples |
|------|---------|----------|
| Number | `-?[0-9]+` | `42`, `-7`, `0` |
| Phrase | `"[^"]*"` | `"Hello"`, `"Aragorn"` |
| Truth | `True` \| `False` | `True`, `False` |

#### 1.2.2 Identifiers
Identifiers begin with an alphabetic character or underscore, followed by alphanumerics or underscores:
```
identifier ::= [a-zA-Z_][a-zA-Z0-9_]*
```

#### 1.2.3 Keywords (Reserved)
The following are reserved and cannot be used as identifiers:

**Declarations:**
- `Let`, `it`, `be`, `known`, `throughout`, `the`, `land`
- `a`, `an`, `named`, `is`, `of`

**Type Runes:**
- `number`, `phrase`, `truth`, `order`, `tome`
- `:whole`, `:phrase`, `:truth` (type annotations)

**Control Flow:**
- `Should`, `fates`, `decree`, `Then`, `Otherwise`
- `Whilst`, `sun`, `doth`, `rise`
- `For`, `each`, `in`
- `abideth`, `where`, `transformed`, `as` 

**Spells (Functions):**
- `By`, `decree`, `elders`, `spell`, `cast`, `upon`
- `Invoke`, `Return`, `with`

**Modules:**
- `From`, `scroll`, `draw`, `all`, `knowledge`, `take`
- `inscribe`, `Unfurl`

**Exceptions:**
- `Try`, `Catch`, `curse`, `as`, `omen`, `Finally`

**I/O:**
- `Inscribe`, `upon`, `reading`, `from`, `Banish`
- `Let`, `proclaimed`

**Streams (2.4+):**
- `scribe`, `bound`, `to`, `Write`, `verse`, `into`
- `Close`, `Read`, `Done`

**Async (Foundation):**
- `Await`, `omen`

**Comparison:**
- `surpasseth`, `remaineth`, `beneath`, `equals`

**Boolean:**
- `and`, `or`, `not`

### 1.3 Operators

| Operator | Meaning | Precedence |
|----------|---------|------------|
| `+` | Addition / Concatenation | 4 |
| `-` | Subtraction | 4 |
| `*` | Multiplication | 5 |
| `/` | Division (truncated) | 5 |
| `%` | Modulo | 5 |
| `=` | Equality | 2 |
| `surpasseth` | Greater than | 3 |
| `remaineth beneath` | Less than | 3 |
| `and` | Logical AND | 1 |
| `or` | Logical OR | 0 |
| `not` | Logical NOT | 6 (unary) |

### 1.4 Whitespace and Comments
Whitespace (space, tab, newline) separates tokens but has no semantic meaning.

**Note:** As of 3.0, Ardent has no comment syntax. All text is interpreted as code or part of phrase literals.

---

## 2. Type System

### 2.1 Primitive Types

| Ardent Type | Type Rune | LLVM Repr | Description |
|-------------|-----------|-----------|-------------|
| Number | `:whole` | `i64` | 64-bit signed integer |
| Phrase | `:phrase` | `i8*` + len | UTF-8 string (immutable) |
| Truth | `:truth` | `i1` | Boolean value |

### 2.2 Composite Types

| Ardent Type | Syntax | Description |
|-------------|--------|-------------|
| Order | `an order named X` | Ordered list of values |
| Tome | `a tome named X` | Key-value mapping |

### 2.3 Type Runes (Annotations)

Type runes provide optional static type information:

```ardent
Let it be known, a number named age :whole is of 25.
Let it be known, a phrase named name :phrase is of "Aragorn".
Let it be known, a truth named brave :truth is of True.
```

**Type Inference Rules:**
1. If a type rune is provided, use that type
2. If initialized with a literal, infer from literal type
3. If neither, type is `Unknown` (dynamic)

### 2.4 Type Compatibility

| From → To | Whole | Phrase | Truth | Unknown |
|-----------|-------|--------|-------|---------|
| Whole | ✓ | concat | ✗ | ✓ |
| Phrase | ✗ | ✓ | ✗ | ✓ |
| Truth | ✗ | concat | ✓ | ✓ |
| Unknown | ✓ | ✓ | ✓ | ✓ |

**Concatenation lifting:** Numbers convert to decimal strings, Truths convert to "True"/"False".

---

## 3. Declarations

### 3.1 Variable Declaration

**Full Form:**
```ardent
Let it be known throughout the land, a <type> named <identifier> is of <expression>.
```

**Short Forms:**
```ardent
Let it be known, a number named count is of 5.
Let it be known, a phrase named hero is of "Aragorn".
Let it be known, a truth named brave is of True.
```

**Minimal Form (2.2+):**
```ardent
let <identifier> be <expression>
```

### 3.2 Order (List) Declaration

```ardent
Let it be known, an order named heroes is of ["Aragorn", "Legolas", "Gimli"].
```

**Operations:**
- `Expand the order <name> with <value>` — append
- `Amend entry <index> of <name> to <value>` — update
- `Remove entry <index> from <name>` — delete

**Index Assignment (3.1+):**
```ardent
<order>[<index>] be <value>
```

**Filtering (3.1+):**
```ardent
<order> where <predicate>   — returns new order with matching elements
```
The implicit variable `it` refers to each element during filtering.

**Transformation (3.1+):**
```ardent
<order> transformed as <expression>   — returns new order with mapped elements
```
The implicit variable `it` refers to each element during transformation.

### 3.3 Tome (Map) Declaration

```ardent
Let it be known, a tome named hero is of {"name": "Aragorn", "title": "King"}.
```

**Operations:**
- `Expand the tome <name> with <key> as <value>` — insert
- `Amend entry <key> of <name> to <value>` — update
- `Erase entry <key> from <name>` — delete

---

## 4. Expressions

### 4.1 Arithmetic Expressions

```
<expr> ::= <term> (('+' | '-') <term>)*
<term> ::= <factor> (('*' | '/' | '%') <factor>)*
<factor> ::= <literal> | <identifier> | '(' <expr> ')' | <call>
```

**Semantics:**
- Addition, subtraction, multiplication on integers
- Division truncates toward zero
- Division by zero raises a curse

### 4.2 Comparison Expressions

```ardent
<a> surpasseth <b>        — a > b
<a> remaineth beneath <b> — a < b
<a> equals <b>            — a == b
```

### 4.3 Boolean Expressions

```ardent
<a> and <b>    — logical AND
<a> or <b>     — logical OR
not <a>        — logical NOT
```

### 4.4 Phrase Concatenation

When either operand is a phrase, `+` performs concatenation with spacing:
1. Convert non-phrase operands to phrases
2. If left ends with space OR right begins with punctuation, no extra space
3. Otherwise, insert single space
4. Collapse duplicate spaces

---

## 5. Control Flow

### 5.1 Conditionals

**If-Then:**
```ardent
Should the fates decree <condition>:
    <body>
```

**If-Then-Else:**
```ardent
Should the fates decree <condition>:
    <then-body>
Otherwise:
    <else-body>
```

### 5.2 Loops

**While Loop:**
```ardent
Whilst the sun doth rise and <condition>:
    <body>
Done
```

**For-Each Loop:**
```ardent
For each <var> in <collection>:
    <body>
Done
```

**For-Each with Key-Value (3.1+ Tome iteration):**
```ardent
For each <key>, <value> in <tome>:
    <body>
Done
```

**Membership Test (3.1+):**
```ardent
<value> abideth in <collection>
```
Returns `True` if the value exists in the order, or if the key exists in the tome.

**Ascending Range:**
```ardent
Ascend from <start> to <end>:
    <body>
```

**Descending Range:**
```ardent
Descend from <start> to <end>:
    <body>
```

---

## 6. Spells (Functions)

### 6.1 Spell Declaration

```ardent
By decree of the elders, a spell named <name> is cast upon <param1> known as <type1>, <param2> known as <type2>:
    <body>
    Return with <expression>.
```

**Minimal Form:**
```ardent
spell <name>(<params>):
    <body>
```

### 6.2 Spell Invocation

```ardent
Invoke the spell <name> upon <arg1>, <arg2>
```

### 6.3 Purity Rules

A spell is **pure** if it:
1. Has no side effects (no I/O, no global mutation)
2. Returns the same output for the same inputs
3. Does not call impure spells

**Impure operations:**
- `Let it be proclaimed` (I/O)
- `Inscribe upon` (file I/O)
- `Invoke the spirit of` (native calls)
- Stream operations (scribe read/write/close)
- `time.now`, `time.sleep`, `time.measure`

Pure spells are candidates for inlining and memoization.

---

## 7. Modules (Scrolls)

### 7.1 Import All

```ardent
From the scroll of "<name>" draw all knowledge.
```

### 7.2 Selective Import

```ardent
From the scroll of "<name>", take <spell1>, <spell2>.
```

### 7.3 Inscribe (Short Import)

```ardent
inscribe "<name>"
inscribe "<name>@<version>"
```

---

## 8. Exception Handling

### 8.1 Try-Catch-Finally

```ardent
Try:
    <risky-body>
Catch the curse as <omen-var>:
    <handler-body>
Finally:
    <cleanup-body>
```

### 8.2 Curse Semantics

Curses propagate up the call stack until caught. Uncaught curses terminate execution with an error message.

**Built-in Curses:**
- Division by zero
- Undefined variable access
- Type mismatch in strict mode
- File I/O errors
- Index out of bounds

---

## 9. File I/O

### 9.1 Legacy File Operations

```ardent
Inscribe upon "<path>" the words "<content>".
Reading from "<path>" into <var>.
Banish the scroll "<path>".
```

### 9.2 Stream I/O (2.4+)

**Open:**
```ardent
Let a scribe named <name> be bound to "<path>"
```

**Write:**
```ardent
Write the verse <expression> into <scribe>
```

**Read:**
```ardent
Read from scribe "<path>" as <var>:
    <body>
Done
```

**Close:**
```ardent
Close the scribe <name>
```

---

## 10. Native Interface

### 10.1 Spirit Invocation

```ardent
Invoke the spirit of <module>.<function> upon <args>
```

**Standard Spirits:**
- `math.add`, `math.subtract`, `math.multiply`, `math.divide`
- `math.modulo`, `math.power`, `math.sqrt`
- `string.length`, `string.upper`, `string.lower`, `string.trim`
- `time.now`, `time.sleep`, `time.sleep_ms`, `time.measure`
- `io.read`, `io.write`, `io.exists`

**Collection Spirits (3.1+):**
- `order.keys(<tome>)` — returns an order of all keys in a tome
- `order.new(<size>, <default>)` — creates a new order of given size with default value
- `order.append(<order>, <value>)` — appends value to order, returns new order
- `has_key(<tome>, <key>)` — returns True if key exists in tome

---

## 11. Async Foundation (2.4+)

### 11.1 Await Syntax

```ardent
Await the omen of <expression>
```

**Current behavior:** Executes synchronously.  
**Future behavior:** Will yield to scheduler and resume when result is ready.

---

## 12. Reserved for Future

The following constructs are reserved and MUST NOT be used:

- `spawn`, `async`, `yield` — future async primitives
- `trait`, `impl` — future type classes
- `match`, `case` — future pattern matching
- `macro`, `quote`, `unquote` — future metaprogramming
- `unsafe` — future low-level escape hatch

---

## 13. Deprecated Constructs

The following are deprecated and will be removed in 4.0:

| Deprecated | Replacement | Reason |
|------------|-------------|--------|
| `winters` suffix | plain number | Archaic |
| `conjoined with` | `+` operator | Verbose |

---

## 14. ABI Contract (AOT)

### 14.1 ArdentValue Structure

```c
struct ArdentValue {
    int8_t tag;      // 0=Number, 1=Phrase, 2=Truth
    int8_t _pad[7];  // alignment
    union {
        int64_t num;
        const char* str;
        bool truth;
    };
};
```

### 14.2 ArdentValueLL (LLVM ABI)

```c
struct ArdentValueLL {
    int32_t tag;
    int64_t num;
    int8_t truth;
    const char* str;
    int32_t len;
};
```

### 14.3 Runtime Functions

All runtime functions use C linkage (`extern "C"`):

```c
ArdentValue ardent_rt_add(ArdentValue a, ArdentValue b);
ArdentValue ardent_rt_print(ArdentValue v);
void ardent_rt_print_av_ptr(const ArdentValueLL* v);
void ardent_rt_concat_av_ptr(const ArdentValueLL* a, const ArdentValueLL* b, ArdentValueLL* out);
int64_t ardent_rt_add_i64(int64_t a, int64_t b);
int64_t ardent_rt_sub_i64(int64_t a, int64_t b);
int64_t ardent_rt_mul_i64(int64_t a, int64_t b);
int64_t ardent_rt_div_i64(int64_t a, int64_t b);
```

---

## 15. Conformance

An implementation is **conformant** if:
1. It accepts all valid programs per this spec
2. It rejects all syntactically invalid programs
3. It produces identical output for deterministic programs
4. It implements all required runtime functions
5. It maintains type safety guarantees in AOT mode

---

## Appendix A: Grammar (Complete EBNF)

```ebnf
program      ::= prologue? declaration*
prologue     ::= 'Prologue:' metadata*
metadata     ::= IDENTIFIER ':' value NEWLINE

declaration  ::= var-decl | spell-decl | statement

var-decl     ::= 'Let' 'it' 'be' 'known' ','? type-clause? 'named' IDENTIFIER type-rune? 'is' 'of' expression '.'?

type-clause  ::= 'throughout' 'the' 'land' ','?
               | 'a' ('number' | 'phrase' | 'truth')
               | 'an' 'order'
               | 'a' 'tome'

type-rune    ::= ':whole' | ':phrase' | ':truth'

spell-decl   ::= 'By' 'decree' 'of' 'the' 'elders' ',' 'a' 'spell' 'named' IDENTIFIER 
                 'is' 'cast' 'upon' param-list ':' block

param-list   ::= param (',' param)*
param        ::= IDENTIFIER 'known' 'as' type-name

statement    ::= if-stmt | while-stmt | for-stmt | return-stmt | try-stmt
               | print-stmt | invoke-stmt | import-stmt | io-stmt
               | stream-stmt | expression-stmt

if-stmt      ::= 'Should' 'the' 'fates' 'decree' expression ':' block ('Otherwise' ':' block)?

while-stmt   ::= 'Whilst' 'the' 'sun' 'doth' 'rise' 'and' expression ':' block

for-stmt     ::= 'For' 'each' IDENTIFIER 'in' expression ':' block
               | 'For' 'each' IDENTIFIER ',' IDENTIFIER 'in' expression ':' block  // 3.1 key-value

index-assign ::= postfix-expr '[' expression ']' 'be' expression  // 3.1

return-stmt  ::= 'Return' 'with' expression '.'?

try-stmt     ::= 'Try' ':' block 'Catch' 'the' 'curse' 'as' IDENTIFIER ':' block ('Finally' ':' block)?

print-stmt   ::= 'Let' 'it' 'be' 'proclaimed' ':'? expression '.'?

invoke-stmt  ::= 'Invoke' 'the' 'spell' IDENTIFIER 'upon' arg-list
               | 'Invoke' 'the' 'spirit' 'of' IDENTIFIER '.' IDENTIFIER 'upon' arg-list

import-stmt  ::= 'From' 'the' 'scroll' 'of' STRING ('draw' 'all' 'knowledge' | ',' 'take' name-list) '.'?
               | 'inscribe' STRING

io-stmt      ::= 'Inscribe' 'upon' STRING 'the' 'words' expression '.'?
               | 'Reading' 'from' STRING 'into' IDENTIFIER '.'?
               | 'Banish' 'the' 'scroll' STRING '.'?

stream-stmt  ::= 'Let' 'a' 'scribe' 'named' IDENTIFIER 'be' 'bound' 'to' STRING
               | 'Write' 'the' 'verse' expression 'into' IDENTIFIER
               | 'Close' 'the' 'scribe' IDENTIFIER
               | 'Read' 'from' 'scribe' STRING 'as' IDENTIFIER ':' block 'Done'

expression   ::= or-expr
or-expr      ::= and-expr ('or' and-expr)*
and-expr     ::= not-expr ('and' not-expr)*
not-expr     ::= 'not' not-expr | comparison
comparison   ::= contains-expr (comp-op contains-expr)?
contains-expr ::= additive ('abideth' 'in' additive)?  // 3.1 membership
comp-op      ::= 'surpasseth' | 'remaineth' 'beneath' | 'equals'
additive     ::= term (('+' | '-') term)*
term         ::= factor (('*' | '/' | '%') factor)*
factor       ::= postfix | literal | IDENTIFIER | call | '(' expression ')'
postfix      ::= factor ('where' expression | 'transformed' 'as' expression)*  // 3.1 filter/map
call         ::= IDENTIFIER '(' arg-list? ')'
arg-list     ::= expression (',' expression)*
literal      ::= NUMBER | STRING | 'True' | 'False' | order-lit | tome-lit
order-lit    ::= '[' (expression (',' expression)*)? ']'
tome-lit     ::= '{' (STRING ':' expression (',' STRING ':' expression)*)? '}'

block        ::= INDENT statement+ DEDENT
```

---

## Appendix B: Standard Library Scrolls

| Scroll | Spells | Description |
|--------|--------|-------------|
| `truths` | `affirm`, `deny`, `toggle` | Boolean utilities |
| `numbers` | `abs`, `min`, `max`, `clamp` | Numeric utilities |
| `alchemy` | `transmute`, `blend` | Type conversion |
| `echoes` | `repeat`, `reverse`, `slice` | String utilities |
| `chronicles` | `record`, `recall` | Logging |
| `time` | `now`, `sleep`, `sleep_ms`, `measure` | Time utilities |

---

