# Building native modules
Native modules are needed when you need more performance, low-level hardware access or any other reason for it. Of course you need some experience with PS2SDK and C/C++ coding.  
  
First let's setup our environment in order to expose Athena methods and structures. Open your terminal init file (.bashrc, .profile etc) and add the environment variables below. REMEMBER: Change the dummy path (/path/to/athena/repo/) to where do you store Athena repo.
```bash
export ATHENAENV_SRC=/path/to/athena/repo/AthenaEnv/src
```

Now we can start coding our module, let's do a simple Makefile(remember to change according to your needs):
```makefile
EE_ERL = sample_module.erl
EE_OBJS = main.o

EE_INCS += -I$(ATHENAENV_SRC)/include -I$(ATHENAENV_SRC)/quickjs

all: $(EE_OBJS)
	$(DIR_GUARD)
	$(EE_CC) -nostartfiles -nostdlib $(EE_OPTFLAGS) -o $(EE_ERL) $(EE_OBJS) $(EE_LDFLAGS) $(EXTRA_LDFLAGS) -Wl,-r -Wl,-d
	$(EE_STRIP) --strip-unneeded -R .mdebug.eabi64 -R .reginfo -R .comment $(EE_ERL)

clean:
	rm -f $(EE_ERL) $(EE_OBJS)

include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
```
Then we can put some C/C++ code on it :D
```c
#include <stdint.h>
#include <quickjs.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static uint32_t fibonacci_rec(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fibonacci_rec(n - 1) + fibonacci_rec(n - 2);
}

static JSValue custom_module_fibonacci(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	uint32_t n;
	JS_ToUint32(ctx, &n, argv[0]);

	return JS_NewUint32(ctx, fibonacci_rec(n));
}

static const JSCFunctionListEntry module_functions[] = {
	JS_CFUNC_DEF("fibonacci",  1, custom_module_fibonacci),
};

static int module_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, module_functions, countof(module_functions));
}

// This function is actually very important. Athena calls it during the module initialization routine, so you ALWAYS need to have a function called "js_init_module" on your code, otherwise your module will never be loaded (mainly because you are never passing it to the VM global context) and it will crash athena module loader.
JSModuleDef *js_init_module(JSContext* ctx) {
    JSModuleDef *m = JS_NewCModule(ctx, "CustomModule", module_init);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, module_functions, countof(module_functions));
    return m;
}

// _start routine with return 0 is mandatory.
int _start(int argc, char *argv[]) {
    return 0;
}

```
And, finally, import and test our C code on JS side!
```js
import * as CustomModule from 'sample_module.erl' //the "as" thing is actually important here

const fib_step = CustomModule.fibonacci(5);
console.log(fib_step); // Show on PCSX2 console log window

System.sleep(9999999); // Put the main thread to sleep
```

This example module is available at "native_module_template".