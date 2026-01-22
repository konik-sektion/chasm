
Want to use ChASM? Compile the assembler:

```zsh
gcc -o ./chasmc src/chasmc.c
```

Run the assembler:

```zsh
./chasmc <path/to/chasm/file> -o <output>
```

If the assembler does not have executable permissions:

```zsh I USE ARCH BTW!!!
chmod +x ./chasmc
```
or
```zsh
sudo chmod +x ./chasmc
```

| Extension | Description            |
| --------- | ---------------------- |
| `.chasm`  | ChASM file             |
| `.ravine` | ChASM header           |
| `.csm`    | ChASM file shorthand   |
| `.rvn`    | ChASM header shorthand |
## Assembling Your First Chassis

To assemble your first ChASM file, given the `chasmc` assembler is already compiled, just run:

```bash
./chasmc <path/to/file> -o <output>.asm
```

Later, the ChASM assembler will automatically assemble from ChASM to NASM and link to an executable. For now, you must run NASM and LD separately:

```bash
nasm <path/to/file> -o <output>.o
ld <path/to/file> -o <output>
```

Then you can run the executable. If the file does not have executable permissions:

For Linux (i use arch btw)
```zsh
chmod +x <path/to/file>
```

If need be, use `sudo` or run the command as user `root`.
```zsh
sudo chmod +x <path/to/file>
```
```zsh
[root@host] $ chmod +x <path/to/file>
```
## Program Shape

Every ChASM file is organized around sections, as with ASM. However, the smallest
runnable program still needs a `main` entrypoint function, which must have `global`
visibility:

```chasm
global func main() >> Null:
    return;
end
```

## Using Headers and Modules

ChASM headers (`.ravine`) typically bundle macros, helper data, and some functions.
There can be multiple modules defined within a header, which can each be explicitly
imported, or the whole header can be imported. Its namespace is referenced via
`<namespace>::<name>` if `#uns <namespace>` is not specified in the dependent file.

If a specific module of a header is being imported, then its namespace will be its name:
`<namespace>::<namespace>` (*e.g.* `exit::$exit` from `#import stddef.exit`)

```chasm
#import stddef.exit
#import stdout
#import stdmem
#uns stdmem

global func main() >> Null:
    *py = memalloc(u8, 2);
    stdout::print(*py);
    free(py);
    void(py);
    stdout::print("Hello, world!");
    exit::$exit, 0;
end
```

Headers such as `stddef` are builtin to the ChASM Standard Library and can be accessed without a specified path.

For a more in-depth language overview, see [[Language Overview]].