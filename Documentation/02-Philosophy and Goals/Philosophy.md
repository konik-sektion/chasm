# Philosophy of ChASM

## Core Principles

1. Explicitness over convenience
2. Predictable code generation
3. Human-readable Assembly
4. Zero runtime overhead
5. Direct hardware control

## Design Constraints

- One-to-one or near one-to-one mapping to x86_64 Assembly.
- No hidden allocations!!
- No implicit syscalls!!
- No automatic bounds checks!!

## Why the fuck does this exist??

Answer: Because I hate myself.

Answer 2: because raw Assembly:
- Is difficult to structure
- Is error-prone at scale
- Lacks ergonomics

ChASM introduces *structure* to Assembly. If you want to 
make an abstract function like `std::cout << "Hello, world!" << std:endl;`, well... have fun!
