
The `bss` section is a *subsection* of the `data` section. It contains all the  **uninitialized** data that comprises a program (whereas `data` stores initialized data, *i.e.* data with value). Typically, ChASM programs will also have *some* degree of uninitialized data, since variables <u>cannot be defined</u> in `#section program`, but the `program` can manipulate uninitialized data in `bss`.

```chasm
#import stdout
#uns stdout

#section bss
let y:u64;

#section program
global func main() >> Null:
    y = "Hello, world!";
    printf("%s", y);
end
```
```stdout
Hello, world!
```
