## Core Principles

1. Explicitness over convenience
2. Predictable code generation
3. Human-readable Assembly
4. Zero runtime overhead
5. Direct hardware control

These principles mean that any abstraction in ChASM (such as the for-convenience abstractions
provided by some header files) must still be visible and traceable in the resulting Assembly.
If the programmer cannot deeply explain how the assembler lowers a statement to NASM, the
statement probably doesn't belong in ChASM.

## Design Constraints

- One-to-one or near one-to-one mapping to x86_64 Assembly.
- No hidden allocations!!
- No implicit syscalls!!
- No automatic bounds checks!!
- No surprise lifetime extension for stack data!!
- No runtime type metadata unless you explicitly build it!!
- Only for $9.99 per month!!

ChASM does not aim to hide its complexities. It aims to present its complexities in a cleaner
and more structured manner. I polled several different chad ChASM programmers and the most
agreed upon analogy for this would be using a recipe book (ChASM) versus winging it (Assembly),
given you suck ass at cooking.

## Why the fuck does this exist??

Answer: Because I hate myself.

Answer 2: because raw Assembly:
- Is difficult to structure
- Is error-prone at scale
- Lacks ergonomics

ChASM introduces *structure* to Assembly. If you want to  make an abstract function like `std::cout << "Hello, world!" << std:endl;`, well... have fun! (Except that *is* included in `stddef`. However, don't use `stddef` abstractions if you don't understand how they will work under-the-hood, because then you suck and I hate you.)

## Nongoals

Never expect these.

- Being a safer CL/C replacement
- Providing garbage collection or automatic memory management
- Abstracting away the target architecture
- Having a large runtime or a large standard library out of the box