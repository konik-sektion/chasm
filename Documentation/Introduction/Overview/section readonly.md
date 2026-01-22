
The `readonly` section contains data that can only be read, not written to. It is useful for setting constant data that the program will need in execution.

```chasm
#section readonly
let pi:u8 = 3.1415926535

#section program
local inline func square(num:u8) >> u64:
	return num*num;
end

local inline func area(radius:u8) >> u16:
	return (u16)pi*square(radius);
end

global func main():
	return area(4);
end
```

Data defined as `readonly` would be similar to a `const` variable in languages such as C.

```chasm
#import stddef

#section readonly
let pi:u8 = 3.14

#section program
global func main():
	stddef::printf("%d", pi);
	stddef::$exit, 0;
end
```
```c
#include <stdio.h>

const float pi = 3.14;

int main() {
	printf("%f", pi);
	return 0;
}
```