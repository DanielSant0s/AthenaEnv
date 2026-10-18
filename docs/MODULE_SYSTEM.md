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

## Built-in Relocatable Subsystems (`.erl`)

To keep base EE RAM footprint low (~3.8 MB), large optional AthenaEnv subsystems are built as relocatable `.erl` modules in `bin/` by default and loaded on demand via ES `import`:

| `.erl` Module | Subsystems Included | Example Import |
|---|---|---|
| `athena_network.erl` | `Network`, `Request`, `Socket`, `WebSocket` (lwIP + BearSSL + `NETMAN`/`SMAP`) | `import { Network, Request, Socket, WebSocket } from 'athena_network.erl';` |
| `athena_native.erl` | `Native` (R5900 AOT compiler, `Native.struct`, dynamic arrays) | `import * as Native from 'athena_native.erl';` |
| `athena_archive.erl` | `Archive` (`libzip` + `liblzma` + tar/gz extraction) | `import * as Archive from 'athena_archive.erl';` |
| `athena_mpeg.erl` | `Video` (`libmpeg` MPEG1/2 video decoder) | `import { Video } from 'athena_mpeg.erl';` |

