# Module System

AthenaEnv can import JavaScript or native compiled modules.

## JavaScript Modules

Module writing is pretty straightforward when you are used to JavaScript. Here's an example:
```js
export function fib(n)
{
    if (n <= 0)
        return 0;
    else if (n == 1)
        return 1;
    else
        return fib(n - 1) + fib(n - 2);
}
```
To import it at runtime, just do:
```js
import fib from 'module_code.js'
```

## Native Modules

For native modules, you have to follow a few instructions to write them: see [Native Modules](NATIVE_MODULES.md).

Native module importing is pretty simple:
```js
import * as CustomModule from 'sample_module.erl' //the "as" thing is actually important here
```
