# Introduction
## Volume I
### Scope

This document constitutes the normative specification of the Chassis Assembly
Programming Language (Chasm, stylized ChASM). The present specification defines 
the foundational priciples, formal character, semantic domain, structural
commitments, lexical structure, grammar, typing rules, operational semantics,
intermediate representations, object model, calling conventions, binary
interface, and translation to target architectures.

All statements in this specification using the terms *shall*, *must*, *shall
not*, *must not*, *required*, or *prohibited* or normative. Statements using
the terms *should*, *recommended*, or *may* are advisory and non-normative.

This specification defines ChASM as a deterministic, purely functional,
statically analyzable systems programming language whose compilation target
is native machine code via structures lowering to conventional assembler
(e.g., NASM for x64). ChASM is not defined as an extention of any existing
Assembly language; rather Assembly is defined as a lower semantic stratum
into which ChASM is translated. The specification also stands in testament
to the author's immense and niche form of the 'Tism, as we (Me, Myself, and I)
**all** know no one will read this but the author and the author's pals. It
is yet another excuse to do nothing All Day (song by Kanye West).

---

### Language Identity

ChASM is a structured, purely-functional, low-level programming language.
ChASM programs are composed exclusively of functions and function
applications. Every syntactic construct denotes a function or the application
of a function. No construct in ChASM denotesmutable state in the imperative
sense. State, when represented, is encoded as an explicit value that is
transformed through functional composition.

ChASM is therefore defined by the following axioms:

1) *Function Totality Axiom* - Every syntactic construct in ChASM denotes a function.
2) *Immutability Axiom* - No binding in ChASM may be mutated after definition.
3) *Explicit State Axiom* - Any interaction with machine state shall be represented as a value passed through and returned from functions.
4) *Determinism Axiom* - Evaluation of a well-typed ChASM program is deterministic.

ChASM shall not contain statements in the imperative sense. There are no
assignment statements, mutation operators, or hidden state transitions. Any
appearance of assignment in surface syntax shall be defined as syntactic sugar
for functional binding.

---

### Computational Model

ChASM defines computation as the transformation of values through function
application over an explicitly modeled machine state.

Let:

- `V` be the set of values.
- `S` be the abstract machine state.
- `F` be the set of ChASM functions.

A ChASM function is defined formally as:

$$
f\ :\ \left(V_1 \times V_2 ... \times V_n \times S\right) \to \left(V \times S\right)
$$

That is, every function consumes zero or more values and an explicit state
value, and produces a value and a new state.

For pure computational functions that do not observe or modify machine state,
the state parameter is functionally preserved:

$$
f\ :\ \left(V_1 \times ... \times V_n \times S) \to (V \times S\right)
$$
$$
\text{where}\ S_{out} = S_{in}
$$

All observable side effects (e.g., syscalls, I/O memory operations, etc.) are
represented as state transformations.

---

### Execution Semantics

A ChASM program is defined as a closed function:

$$
start\ : \ S_0 \to S_f
$$

where $`S_e`$ is the initial machine state sullied by the runtime environment
and $`S_f`$ is the final machine state returned by evaluation.

Program execution is defined as evaluation of `start` under the operational
semantics defined in Volume III.

This implementation shall lower this functional representation into a sequence
of machine instructions that preserve semantic equivalence with the functional
specification.

---

### Referential Transparency

ChASM is referentially transparent (hence the name of this section...). For any
expression $`E`$, replacing $`E`$ with its evaluated value shall not change
program behavior. Formally (for any program context $`C\left[\ \right]`$):

$$
\text{If}\ E \Downarrow v, \text{then}\ C\left[E\right] \Downarrow r \Leftrightarrow C\left[v\rigjt] \Downarrow r
$$

This property is required for all well-typed programs.

