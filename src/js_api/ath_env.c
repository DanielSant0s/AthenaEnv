#include <assert.h>
#include <sys/fcntl.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include <ath_env.h>

#include <memory.h>
#include <erl.h>

#define TRUE 1

#define JSFILE_NOTFOUND -5656

JSModuleDef *athena_push_module(JSContext* ctx, JSModuleInitFunc *func, const JSCFunctionListEntry *func_list, int len, const char* module_name){
    JSModuleDef *m;
    m = JS_NewCModule(ctx, module_name, func);
    if (!m)
        return NULL;
    JS_AddModuleExportList(ctx, m, func_list, len);

	dbgprintf("AthenaEnv: %s module pushed at 0x%x\n", module_name, m);
    return m;
}

static int qjs_eval_buf(JSContext *ctx, const void *buf, int buf_len,
                    const char *filename, int eval_flags)
{
    JSValue val;
    int ret;

    if ((eval_flags & JS_EVAL_TYPE_MASK) == JS_EVAL_TYPE_MODULE) {
        /* for the modules, we compile then run to be able to set
           import.meta */
        val = JS_Eval(ctx, buf, buf_len, filename,
                      eval_flags | JS_EVAL_FLAG_COMPILE_ONLY);
        if (!JS_IsException(val)) {
            js_module_set_import_meta(ctx, val, TRUE, TRUE);
            val = JS_EvalFunction(ctx, val);
        }
    } else {
        val = JS_Eval(ctx, buf, buf_len, filename, eval_flags);
    }
	
    if (JS_IsException(val)) {
        ret = -1;
    } else {
        ret = 0;
    }

	JS_FreeValue(ctx, val);
    return ret;
}

static int qjs_handle_fh(JSContext *ctx, FILE *f, const char *filename, const char *bytecode_filename) {
	char *buf = NULL;
	size_t bufsz;
	size_t bufoff;
	size_t got;
	int rc;
	int retval = -1;

	buf = (char *) malloc(1024);
	if (!buf) {
		if (buf) {
			free(buf);
			buf = NULL;
		}
		return retval;
	}
	bufsz = 1024;
	bufoff = 0;

	/* Read until EOF, avoid fseek/stat because it won't work with stdin. */
	for (;;) {
		size_t avail;

		avail = bufsz - bufoff;
		if (avail < 1024) {
			size_t newsz;
			char *buf_new;

			newsz = bufsz + (bufsz >> 2) + 1024;  /* +25% and some extra */
			if (newsz < bufsz) {
				if (buf) {
					free(buf);
					buf = NULL;
				}
				return retval;
			}
			buf_new = (char *) realloc(buf, newsz);
			if (!buf_new) {
				if (buf) {
					free(buf);
					buf = NULL;
				}
				return retval;
			}
			buf = buf_new;
			bufsz = newsz;
		}

		avail = bufsz - bufoff;

		got = fread((void *) (buf + bufoff), (size_t) 1, avail, f);

		if (got == 0) {
			break;
		}
		bufoff += got;
	}

	buf[bufoff++] = 0;
        js_std_add_helpers(ctx, 0, NULL);
        {
            const char *str = athena_js_globals_prelude();
            rc = qjs_eval_buf(ctx, str, strlen(str), "<input>", JS_EVAL_TYPE_MODULE);
            if (rc != 0) { return retval; }
        }
	rc = qjs_eval_buf(ctx, (void *) buf, bufoff - 1, filename, JS_EVAL_TYPE_MODULE);
	free(buf);
	buf = NULL;
	if (rc != 0) { return retval; } else { return 0; }
	return retval;
}

static int qjs_handle_file(JSContext *ctx, const char *filename, const char *bytecode_filename) {
	FILE *f = NULL;
	int retval;

	f = fopen(filename, "r");
	if (!f) {
		//fprintf(stderr, "failed to open source file: %s\n", filename);
		//fflush(stderr);
		return JSFILE_NOTFOUND;
	}

	retval = qjs_handle_fh(ctx, f, filename, bytecode_filename);

	fclose(f);
	return retval;
}

static JSContext *JS_NewCustomContext(JSRuntime *rt)
{
    JSContext *ctx;
    ctx = JS_NewContext(rt);
    if (!ctx)
        return NULL;
    /* system modules */
    js_init_module_std(ctx, "std");
    js_init_module_os(ctx, "os");

	athena_js_register_modules(ctx);

    return ctx;
}

static char error_buf[4096];

void destroy_vm(JSContext* ctx) {
	JSRuntime* rt = JS_GetRuntime(ctx);

	athena_task_free(ctx);

	#ifdef ATHENA_NATIVE_COMPILER
	athena_native_cleanup();
	#else
	{
		struct symbol_t *sym = erl_find_symbol("athena_native_cleanup");
		if (sym && sym->address) {
			((void (*)(void))sym->address)();
		}
	}
	#endif

	js_std_free_handlers(rt);
	JS_FreeContext(ctx);
	JS_FreeRuntime(rt);
}

static jmp_buf vm_reset_buf;

jmp_buf *get_reset_buf() {
	return &vm_reset_buf;
}

const char* run_script(const char* script, bool isBuffer)
{
	size_t memoryLimit = (GetMemorySize() - get_used_memory()) >> 1;

    dbgprintf("\nStarting AthenaEnv...\n");
    JSRuntime *rt = JS_NewRuntime(); if (!rt) { return "AthenaError: Runtime creation"; }
    js_std_set_worker_new_context_func(JS_NewCustomContext);
    js_std_init_handlers(rt);

	JS_SetMemoryLimit(rt, memoryLimit);
	JS_SetGCThreshold(rt, memoryLimit - 4194304);

    JSContext *ctx = JS_NewCustomContext(rt); if (!ctx) { return "AthenaError: Context creation"; }

	JS_SetModuleLoaderFunc(rt, NULL, js_module_loader, NULL);

	while (!bootlogo_finished() && boot_logo) {
		usleep(1000);
	}

    int s = qjs_handle_file(ctx, script, NULL);

	if (s >= 0) {
		s = js_std_loop(ctx);
	}

    if (s < 0) { 
		if (s == JSFILE_NOTFOUND) {
			sprintf(error_buf, "AthenaError: Fail when opening %s\n"
							   "\nTip: If you are on PCSX2, check Host filesystem!\n", script);
		} else {
			JSValue exception_val = JS_GetException(ctx);
			const char* exception = JS_ToCString(ctx, exception_val);
			JSValue stack_val = JS_GetPropertyStr(ctx, exception_val, "stack");
			const char* stack = JS_ToCString(ctx, stack_val);
			JS_FreeValue(ctx, exception_val);
			JS_FreeValue(ctx, stack_val);

			strcpy(error_buf, exception);
			strcat(error_buf, "\n");
			strcat(error_buf, stack);
		}
		
		destroy_vm(ctx);

		return error_buf; 
	}
	
	destroy_vm(ctx);

    return NULL;
}
