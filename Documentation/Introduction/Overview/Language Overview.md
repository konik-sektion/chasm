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

[[section data]] [[section program]] [[section bss]] [[section macros]] [[section readonly]]
![[section data]] 

Reference [[Quick Reference - Variables]] for more on variables in `#section data`.
![[section program]] ![[section bss]] ![[section macros]] ![[section readonly]]