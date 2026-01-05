#include <stdint.h>
#include <quickjs.h>

#define countof(x) (sizeof(x) / sizeof((x)[0]))

static uint32_t fibonacci_rec(int n) {
    if (n <= 0) return 0;
    if (n == 1) return 1;
    return fibonacci_rec(n - 1) + fibonacci_rec(n - 2);
}

static JSValue custom_module_fibonacci(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t n;
	JS_ToUint32(ctx, &n, argv[0]);

	return JS_NewUint32(ctx, fibonacci_rec(n));
}

static const JSCFunctionListEntry module_functions[] = {
	JS_CFUNC_DEF("fibonacci",  1, custom_module_fibonacci),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_functions, countof(module_functions));
}

JSModuleDef *js_init_module(JSContext* ctx){
    JSModuleDef *m = JS_NewCModule(ctx, "CustomModule", module_init);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, module_functions, countof(module_functions));
    return m;
}

int _start(int argc, char *argv[]) {
    return 0;
}
