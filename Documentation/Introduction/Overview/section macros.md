
The `macros` section is used to define macros. Macros are defined with the `def` keyword and terminated with the `enddef` keyword. Macros help simplify code drastically, with things like `$exit, <code>;` simplifying the amount of inline Assembly or long ChASM functions the user  would have to otherwise write. Macros defined in the `macros` section can be called with `$<macro>, [args];`. Macros in theory *behave the same* as inline functions; with their code being inserted at the point of invocation, but have some differences under-the-hood.

The `macros` section is used to define macros. Macros are *similar* to inline functions in the sense that they will be *expanded at the point of invocation*.  However, inlines and macros have their differences.

A macro can be defined with the `def` keyword. `def` is only available under the `macro` section; attempting to `def` under any other section is invalid and will raise `Fatal: MacroDefinition` and crash the program.

Similarly to the definition of functions being terminated with `end`, the definition of macros must be ended with `enddef`. Not doing so could raise a wide variety of `Fatal`s, namely `Fatal: SyntaxError`.

```chasm
#section macros
def hello_world, 0:
	stdout::print("Hello, world!");
enddef
```

Later in the program, when the `hello_world` macro is invoked:

```chasm
$hello_world,;
```
```stdout
Hello, world!
```

What actually happens is the macro invocation is replaced by the content of the macro.
$$\text{\$<macro-name>} \to \text{<macro-contents>}$$  
$$ \text{\$hello\_world} \to \text{stdout::print("Hello, world!");} $$
In this way `inline func <func>` and `def <macro>` are similar. See [[Functions]] for more.