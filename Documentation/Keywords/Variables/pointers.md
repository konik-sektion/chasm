
Pointers in ChASM are identical to pointers in C. A pointer is a variable that simply points to an address in memory.

```chasm
#import stddef
#import stdout

#section data

let x:u8 = 2;
*ptr = &x;

#section program

global func main() >> Null:
	stdout::print(x);
	stdout::print(*ptr);
	
	$exit, 0;
end
```

### Things to know

- Pointers can point to any number of other pointers.
- A pointer can manipulate values stored in memory.
- A pointer should be `void`ed when the value it points to is `void`ed or becomes `Null`.
- Do not reference a `Null` pointer.

### Manipulation of values in memory

Say we have a pointer `ptr` that points to the address of a variable `x`.

```chasm
let x:u8 = 1;
let *ptr = &x;
```

Then `x` is `push`ed to stack and the variable undeclared:

```chasm
push x;
```

Now the programmer may try printing the value of `x`.

```chasm
stdout::print(x);
```
```stdout
Fatal: SyntaxError: undefined token 'x' at line <number>
```

Since `x` is undeclared, it is no longer recognized by the program. The programmer may then attempt to print the value of `ptr` by dereferencing it.

```chasm
stdout::print(*ptr);
```
```stdout
1
```

Since the variable `x` was pushed to stack before being `void`ed, it is still referenced by the pointer `ptr`. Because of this, variables on the stack (or the heap) can be manipulated through their pointers.