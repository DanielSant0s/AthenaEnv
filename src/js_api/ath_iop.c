#include <unistd.h>
#include <ath_env.h>
#include <iop_manager.h>

#include <smem.h>

#include <macros.h>

static JSValue athena_sifregistermodule(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	void *data = NULL;
	size_t size = 0;
	uint8_t dependencies[4] = { EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY, EMPTY_ENTRY };

	JSValue init_fun = NULL, end_fun = NULL;

	iopman_func init = NULL, end = NULL;

	char* name = JS_ToCStringLen(ctx, &size, argv[0]);

	if (JS_IsString(argv[1])) {
		data = JS_ToCString(ctx, argv[1]);
	} else {
		data = JS_GetArrayBuffer(ctx, &size, argv[1]);
	}

	if (argc > 2) {
		JSValue arg_array = JS_GetPropertyStr(ctx, argv[2], "deps");

		uint32_t num_deps = 0;
		JS_ToUint32(ctx, &num_deps, JS_GetPropertyStr(ctx, arg_array, "length"));
	
		uint32_t dep_id = EMPTY_ENTRY;
		for (int i = 0; i < num_deps; i++) {
			JS_ToUint32(ctx, &num_deps, JS_GetPropertyUint32(ctx, arg_array, i));
			dependencies[i] = dep_id;
		}

		JS_FreeValue(ctx, arg_array);

		init_fun = JS_GetPropertyStr(ctx, argv[2], "init");

		if (JS_IsFunction(ctx, init_fun)) {
			init = lambda(int, (module_entry *module) {
				JS_Call(ctx, (JSValue)module->init_args, JS_UNDEFINED, 0, NULL);
				return 0;
			});
		} else if (JS_IsNumber(init_fun)) {
			JS_ToUint32(ctx, &init, init_fun);
		}

		end_fun = JS_GetPropertyStr(ctx, argv[2], "end");

		if (JS_IsFunction(ctx, end_fun)) {
			end = lambda(int, (module_entry *module) {
				JS_Call(ctx, (JSValue)module->end_args, JS_UNDEFINED, 0, NULL);
				return 0;
			});
		} else if (JS_IsNumber(end_fun)) {
			JS_ToUint32(ctx, &end, end_fun);
		}
	}

	module_entry *result = iopman_register_module(name, data, size, dependencies, init, end);

	if (init_fun)
		result->init_args = (void*)init_fun;

	if (end_fun)
		result->end_args = (void*)end_fun;

	return JS_NewInt32(ctx, result);
}

static JSValue athena_sifloadmodule(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	module_entry *module = NULL;

	if (JS_IsString(argv[0])) {	
		char *module_name = JS_ToCString(ctx, argv[0]);
		module = iopman_search_module(module_name);

		JS_FreeCString(ctx, module_name);
	} else {
		JS_ToUint32(ctx, &module, argv[0]);
	}

	if (module) {
		int arg_len = 0;
		const char *args = NULL;
	
		if(argc == 3){
			JS_ToInt32(ctx, &arg_len, argv[1]);
			args = JS_ToCString(ctx, argv[2]);
		}

		int ret = iopman_load_module(module, arg_len, args);
		if (ret == MODULE_STATUS_INCOMPATIBILITY) {
			return JS_ThrowInternalError(ctx, "loadModule: %s module is incompatible with %s, which is actually loaded. Keep or reset IOP.", 
					module->name, 
					iopman_get_incompatible_module()->name);
		} else if (ret == MODULE_STATUS_ERROR) {
			return JS_ThrowInternalError(ctx, "loadModule: error while loading %s.", module->name);
		}
	} else {
		return JS_ThrowInternalError(ctx, "loadModule: module not found.");
	}
		
	return JS_UNDEFINED;
}

static JSValue athena_sifgetmodule(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	module_entry *module = NULL;

	if (JS_IsString(argv[0])) {	
		char *module_name = JS_ToCString(ctx, argv[0]);
		module = iopman_search_module(module_name);

		JS_FreeCString(ctx, module_name);

		if (module)
			return JS_NewUint32(ctx, (uint32_t)module);
	} else {
		uint32_t id = 0xFF;
		JS_ToUint32(ctx, &id, argv[0]);
		return JS_NewUint32(ctx, (uint32_t)iopman_get_module(id));
	}
		
	return JS_UNDEFINED;
}

static JSValue athena_sifgetmodules(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	uint32_t list_size = 0;
	module_entry *modules = iopman_get_modules(&list_size);

	if (list_size) {	
		JSValue arr = JS_NewArray(ctx);

		for (int i = 0; i < list_size; i++) {
			JSValue obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, modules[i].name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "id", JS_NewUint32(ctx, modules[i].id), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
		}

		return arr;
	}
		
	return JS_UNDEFINED;
}

static JSValue athena_resetiop(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	iopman_reset();

	return JS_UNDEFINED;
}

static JSValue athena_getiopmemory(JSContext *ctx, JSValue this_val, int argc, JSValueConst * argv){
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "Wrong number of arguments");
	s32 freeram = 0;
    s32 usedram = 0;

	iopman_load_module(iopman_search_module("freeram"), 0, NULL);

    smem_read((void *)(1 * 1024 * 1024), &freeram, 4);
    usedram = (2 * 1024 * 1024) - freeram;

	JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "free", JS_NewUint32(ctx, freeram), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "used", JS_NewUint32(ctx, usedram), JS_PROP_C_W_E);

	return obj;
}

static const JSCFunctionListEntry sif_funcs[] = {
	JS_CFUNC_DEF("newModule",             3, 		athena_sifregistermodule),
	JS_CFUNC_DEF("loadModule",     		  3,        athena_sifloadmodule),
	JS_CFUNC_DEF("getModule",     		  1,        athena_sifgetmodule),
	JS_CFUNC_DEF("getModules",     		  0,        athena_sifgetmodules),
	JS_CFUNC_DEF("reset",      			  0,     	athena_resetiop),
	JS_CFUNC_DEF("getMemoryStats",        0,     	athena_getiopmemory),
};

static int sif_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, sif_funcs, countof(sif_funcs));
}

JSModuleDef *athena_iop_init(JSContext* ctx){
	return athena_push_module(ctx, sif_init, sif_funcs, countof(sif_funcs), "IOP");
}

