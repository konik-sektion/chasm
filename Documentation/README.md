# ChASM

*Welcome to ChASM. This README is not intended for anyone in particular,
because I don't really expect ChASM to be used outside of its scope.*

Chasm (stylized **ChASM**) is a low-level human-readable systems programming
language designed for the x86_64 architecture and ZirconiumOS environment.

Although ChASM is a compiled language, the `chasmc` compiler is actually referred
to as the ChASM assembler, because I want it to be that way!

ChASM occupies a niche *only slightly above* traditional Assembly languages 
such as NASM or GAS, providing:
- Structured functions;
- Typed variables;
- Macros and headers;
- Minimal operators; and,
- Explicit system control.

ChASM is **<u>not</u>** a high-level language, to sound like a broken record; 
it is intended only to make Assembly less *painful*, not abstract.

The documentation provided here in `chasm/Documentation` defines the ChASM language, 
its syntax, semantics, assembly model, and integration with ZirconiumOS and ClamLang. 
