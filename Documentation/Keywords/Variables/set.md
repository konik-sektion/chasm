`set` is an obsolete reserved keyword in ChASM used for **variable reassignment**. The programmer may reassign variables with or without `set`.

```chasm
#section data
let change_me:u8 = 5;

#section program
global func main() >> Null:
	set change_me = 3;
	return;
end
```

### Things to know

- `set` is obsolete and its use is optional.
- `set` can only be written under `#section program` or `#section macros`.
- `set` will crash the program if attempting to set a variable that does not exist.