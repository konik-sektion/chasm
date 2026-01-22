### Heap Allocation

Heap memory can be allocated via the `memalloc()` function. `memalloc()` accepts two arguments, and at minimum **must** have its first argument: `memalloc(size, value)`. It will return a pointer to the allocated memory, which may contain garbage data.

```chasm
*ptr = stdmem::memalloc(u8, 6);
stdout::print(*py);*
```
```stdout
6
```

Because *sizes* exist as *types* in ChASM, we can simply pass in our desired size since they are already predefined. However, if the programmer wants to pass in the size of a variable, but not necessarily calculate the size of that variable himself, he can use the `sizeof()` function. `sizeof()` is defined alongside `memalloc()` in the `stddef` header, which itself exists in the Standard Library. This is particularly useful when passing in that variable *also as the value* (not only the size) sent to that point in memory.

```chasm
*ptr = stdmem::memalloc(sizeof(x), x);
```

If there is not enough memory that is available to the program at the time of running `memalloc()`, the program will raise `NotEnoughMemoryWarning` and return `Null`. The execution *will* continue, as long as the program has sufficient memory to continue running and the program does not rely on the pointer returned by `memalloc()`. The program will crash if it runs completely out of memory. Likely, running out of memory will also cause a system crash.

A rather unique thing the programmer may do is cast the allocated memory pointer to a pointer to a size greater (or smaller) than itself:

```chasm
*ptr = stdmem::(u16 *)memalloc(u8);
stdout::printf("The pointer ptr is %d bytes", sizeof(*ptr));
*peter = stdmem::(u8 *)memalloc(u16);
stdout::printf("The pointer peter is %d bytes", sizeof(*peter)); 
```
```stdout
The pointer ptr is 2 bytes
The pointer peter is 1 bytes
```

Note that if the upper 8 bits (in this example) that is allocated to the pointer may be filled with garbage data, which may influence the value of the intended value. For instance, if we have a value of `8` passed as `u8` into `memalloc()`, then cast it to a `u16`:

```chasm
*ptr = stdmem::(u16 *)memalloc(u8, 8);
```

And the upper 8 bits of the `u16` casting is filled with garbage data:
$$
10110010\ 00001000
$$
The value stored by `*ptr` would become $45,576$, not the intended $8$. The only reason this quirk exists is 1: because `*ptr = stdmem::(i64 *)memalloc(i16, -1*x);` looks advanced, and 2: I'm too lazy to fix non-fatal bugs.
### Contiguous Memory Allocation

The programmer can allocate a contiguous block of memory using the `contalloc(size, num, value)` function. It too, like `memalloc()`, returns a pointer to a block of memory; but `contalloc()` sets the value of the passed size (or its casting) to all zeros. Say we have a `u16` block of data in memory that's filled with trash:
$$
10010101\ 10101110
$$
And the program runs `contalloc()` at that block in memory. Its new value would then be:
$$
00000000\ 00000000
$$
Then, if the value passed in was `3`, for example, the memory's value would then be:
$$
00000000\ 00000011
$$
The primary difference between `contalloc()` and `memalloc()` is in its arguments. `contalloc()` must be passed a minimum of **two** arguments (though it *can* take three); the *size*, *number of blocks*, and a *value* (which in the case of `contalloc()` is typically an array). `contalloc()` may also have a larger overhead, since it takes time to initialize memory to $0$ in all bits. This is especially prominent in larger sizes.
### Heap Reallocation

We can reallocate memory to a pointer using `realloc(size, value)`. If we are assigning the return value of `realloc()` to a new pointer, the original pointer becomes `Null` and should no longer be referenced. In this case, ensure to `void` that pointer to prevent accidental `Null` referencing.

```chasm
*ptr = stdmem::memalloc(u8);
*new_ptr = stdmem::realloc(u16);
void ptr;
```

(Example of `realloc()` nulling previous pointer):

```chasm
*ptr = stdmem::memalloc(u8);
*new_ptr = stdmem::realloc(16);
stdout::print(*ptr);
```
```stdout
Null
```

It is harmless to print a `Null` value, so the above is allowed. However, trying to operate on `Null` in any way will raise `Fatal: NullReference` and will crash the program.
### Heap Freeing

Heap memory must be released back to the system once it is done being used. Doing so prevents memory leaks. Freeing memory is done with `free(ptr)`:

```chasm
#section program
global func main() >> Null:
	stddef::try:
		*ptr = stdmem::memalloc(u8);
		;;; some code here....
		free(ptr);
		stddef::$exit, 0;
	stddef::except Exception as e:
		free(ptr);
		stdout::printf("Error: %s", e);
		stddef::$exit, 1;
end
```

The exception-handling abstraction `try-except` is provided by the `stddef` header.