# An Overview of ChASM

## Program Example

```chasm
#import stddef
#uns stddef

#section data
let value:u8;

#section program
global func main() >> Null:
    set value:u8 = 5;
    $exit, 0;
end
```

## Notable Keywords

- Sections (`#section`)
- Variables (`let, set`)
- Functions (`func`)
- Macros (`def`)
- Inlines (`@<foo>`)

## Sections

Sections in ChASM are similar to those in x86_64 NASM Assembly:

```chasm
#section data
#section program
#section bss
#section macros
#section readonly
```

### `#section data`

The `data` section is used to define initialized data. Variables may be `let`ed under
the `program` section, but this is not recommended, as it can lead to unpredictable
behavior. Later versions of the ChASM assembler will lead to an error being raised
in assembly if variables are initialized in `program`, as it is a subsection of `readonly`.

Variables defined in the `data` section are initialized with specific values. Differing
from higher-level languages, in ChASM, variables are typed by *size* and *signedness*.
For instance, we can `let` and ***unsigned 8-bit*** integer `x` with the following:

```chasm
#section data
let x:u8 = 4;
```

`let` statements have the following anatomy:

`let <name> : <type> = <value>;`

Colons are used for typing in ChASM, which is resemblant to Python. More similarly to
Clam languages, however, return typepointers remain the same rarrow:

```chasm
#section program
global func main() >> u8:
    ;;; some code here...
    return 0;
end
```

Lines in ChASM are terminated with a semi-colon, given the line does not end with
a keyword, ends with opening grouping characters `( [ {`defines a function, defines 
a macro, or is a directive.

The variables defined in the `data` section have a static storage duration, and will
only be destroyed when `void`ed. They retain their values between function calls.

```chasm
#import stddef
#uns stddef

#section data
let x:u8 = 1;
let y:u8 = x;

#section program
local func add(num1:u8, num2:u8) >> u8:
    return num1 + num2;
end

global func main() >> Null:
    $exit, add(x, y);
end
```

The variables `x` and `y` will exist for *the entirety of the program's execution.*
However, as also aforementioned, they can still be destroyed at execution.

Variables ***can*** still be initialized at runtime without running into 
the problems mentioned before. This is by use of the stack and heap.

```chasm
#import stddef
#uns stddef

#section data
let x:u8 = 1;
let y:u8 = x;

#section program
local func add(num1:u8, num2:u8) >> u8:
    return num1 + num2;
end

global func main() >> Null:
    push x, y;
    void x, y;
    ;;; now x and y are destroyed, but exist in the stack.
    ;;; we can pop x and y back off of the stack now.
    ;;; note that in pushing variables to the stack,
    ;;; metadata to its value, such as its size, is
    ;;; not retained, but the size is still there.
    ;;; we can use :auto or :<same size here> to
    ;;; retype the variable.
    pop x:auto; ;;; same as x:u8
    pop y:auto; ;;; same as y:u8
end
```

Variables may also be manipulated in *stack* **and** on *heap*. Similarly to C, ChASM uses pointers
to achieve this. A variable `x` can be pointed to by a variable `*ptr`:

```chasm
#import stdout
#import stddef
#import stdmem

#section data
let x:u8 = 10;
let y:u8 = 4;
let *px = &x;

#section bss
let *py:resb 8;
let z:resb 1; 

#section program
local func add(num1:u8, num2:u8) >> u16;
    return num1 + num2;
end

global func main() >> Null:
    *py = stdmem::memalloc(u8, y);
    void y; ;;; now y is undeclared
    stdout::print("x = ", x); ;;; x = 10
    push x;
    ;;; pushing a variable to stack removes it from the program,
    ;;; thus void is not needed
    z = add(*px, *py);
    stdout::print("z = ", z); ;;; z = 14
    px = 20;
    pop x;
    stdout::print("x = ", x); ;;; x = 20
    y = *py;
    free(py);
    void py;
    stdout::print("y = ", y); ;;; 4
end
```

Variables can also be made in execution with `push as`, *e.g.* `push 20 as x; pop x; $print, x;`
would lead to a stdout of `20`.

Heap memory allocated by `memalloc(size, value);` must be freed with `free();`. `memalloc()` returns
a pointer to the allocated memory block. The program will raise `NotEnoughMemoryWarning` if there
is insufficient memory to complete the `memalloc` operation, and the function will instead return
`Null`. Memory can be reallocated with `realloc(size);`

### `#section program`

The `program` section is a *subsection* of the `readonly` section. It contains **read-only**
<u>executable instructions</u> that comprise the program (whereas `data` stores data). Typically,
all programs written in ChASM will have a `program` section, *unless* the file is a header
for macros, data, et cetera.

```chasm
#import stdout
#import stddef
#uns stdout

#section program

global func main() >> Null:
    print("Hello, world!");
    $stddef::exit, 0;
end
```

### `#section bss`

The `bss` section is a *subsection* of the `data` section. It contains all the 
**uninitialized** data that comprises a program (whereas `data` stores initialized
data, *i.e.* data with value). Typically, ChASM programs will also have *some* degree
of uninitialized data, since variables <u>cannot be defined</u> in `#section program`,
but the `program` can manipulate uninitialized data in `bss`.

```chasm
#import stdout
#uns stdout

#section bss
let y:resw 2;

#section program
global func main() >> Null:
    y = "Hello, world!";
    print(y); ;;; Hello, world!
end
```

### `#section macros`

The `macros` section is used to define macros. Macros are defined with the `def` keyword and
terminated with the `enddef` keyword. Macros help simplify code drastically, with things like
*$exit, <code>;* simplifying the amount of inline Assembly or long ChASM functions the user 
would have to otherwise write. Macros defined in the `macros` section can be called with
`$<macro>, [args];`. Macros in theory *behave the same* as inline functions; with their
code being inserted at the point of invocation, but have some differences under-the-hood.

### `#section readonly`

The `readonly` section contains data that can only be read, not written to. It can be used
for things like constant variables. I'm too lazy to come up with other examples, so look
at the source code if you want to learn how to use it better, I guess.

