
The `data` section is used to define initialized data. Variables *may* be `let`ed under
the `program` section, but this is **<u>not</u>** recommended, as it can lead to unpredictable
behavior. Later versions of the ChASM assembler will lead to a `DataInitError` error being raised in assembly if variables are initialized in `program`, as it is a subsection of `readonly`.

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

### Lifespan of Variables

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

The variables `x` and `y` will exist for *the entirety of the program's execution.* However, as also aforementioned, they can still be destroyed at execution.

Variables ***can*** still be initialized at runtime without running into the problems mentioned before. This is by use of the stack and heap. For instance, variables can be made in execution with `push as`, *e.g.* `push 20 as x; pop x; $print, x;` would lead to a stdout of `20`.

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

Variables may also be manipulated in *stack* **and** on *heap*. Similarly to C, ChASM uses pointers to achieve this. A variable `x` can be pointed to by a variable `*ptr`:

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

For more on the manual memory management used on with the heap, see [[Manual Memory Management]].