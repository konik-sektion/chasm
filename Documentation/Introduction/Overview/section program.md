
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
## Directives

Programs contain <u>all</u> of the instructions that will be assembled to executable instructions that the CPU will execute, and are organized into different sections. These sections are defined by section directives by the syntax `#section <name>`. Each assembler directive will determine *where* in memory the following code or data is loaded; `program` for code, `data` for data, et cetera.

The following example demonstrates the use of the most common directives, including `#import` and `#uns`.

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

Without `#uns` (short for "*using namespace*"), the namespace that a function or macro lives in would have to be referenced manually:

```chasm
stddef::$exit, 0;
```