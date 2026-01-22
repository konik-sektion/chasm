
Type annotation in ChASM is performed with the `:` followed by the size to be annotated:

```chasm
let <name>:<size> = <value>;
```

Types in ChASM are defined by **size** and **signedness** rather than arbitrary data types like `str`, `int`, et cetera.

| Type | Size | Description     | Range                                                |
| ---- | ---- | --------------- | ---------------------------------------------------- |
| u8   | 1 B  | Unsigned 8-bit  | 0-255                                                |
| u16  | 2 B  | Unsigned 16-bit | 0-65,535                                             |
| u32  | 4 B  | Unsigned 32-bit | 0-4,294,967,295                                      |
| u64  | 8 B  | Unsigned 64-bit | 0-18,446,744,073,709,551,615                         |
| i8   | 1 B  | Signed 8-bit    | -128-127                                             |
| i16  | 2 B  | Signed 16-bit   | -32,768-32,767                                       |
| i32  | 4 B  | Signed 32-bit   | -2,147,483,648-2,147,483,647                         |
| i64  | 8 B  | Signed 64-bit   | -9,223,372,036,854,775,808-9,223,372,036,854,775,807 |
| Null | 0    | No value        | N/A                                                  |
