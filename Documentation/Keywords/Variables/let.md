`let` is a reserved keyword in ChASM that **initializes** variable data. It can be used under any `data` section, such as `#section data`, `#section bss`, and `#section readonly`.

```chasm
#section data
let x:u8 = 3;
let y:i8 = -4;
```

### Things to know

- `let` cannot be used inside the `program` section.
- `let` will determine the size of the variable (if sized with `:<size>` or `:auto`).
- `let`will initialize a variable with a static lifespan; it will last the duration of the program.
