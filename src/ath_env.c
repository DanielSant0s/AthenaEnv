#include "ath_env.h"

#include <assert.h>
#include <sys/fcntl.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

//ctx stands for duk JavaScript virtual machine stack and values.
duk_context *ctx;

float get_obj_float(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_number(ctx, idx);
}

uint32_t get_obj_uint(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_uint(ctx, idx);
}

int get_obj_int(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_int(ctx, idx);
}

bool get_obj_boolean(duk_context* ctx, duk_idx_t idx, const char* key){
	duk_push_this(ctx);
    duk_get_prop_string(ctx, idx, key);
	return duk_to_boolean(ctx, idx);
}

void push_athena_module(duk_c_function func, const char *key){
	printf("AthenaEnv: Pushing %s module...\n", key);
	duk_push_c_function(ctx, func, 0);
    duk_call(ctx, 0);
    duk_put_global_string(ctx, key);
}

static duk_ret_t wrapped_compile_execute(duk_context *ctx, void *udata) {
	const char *src_data;
	duk_size_t src_len;
	duk_uint_t comp_flags;

	(void) udata;

	/* XXX: Here it'd be nice to get some stats for the compilation result
	 * when a suitable command line is given (e.g. code size, constant
	 * count, function count.  These are available internally but not through
	 * the public API.
	 */

	/* Use duk_compile_lstring_filename() variant which avoids interning
	 * the source code.  This only really matters for low memory environments.
	 */

	/* [ ... bytecode_filename src_data src_len filename ] */

	src_data = (const char *) duk_require_pointer(ctx, -3);
	src_len = (duk_size_t) duk_require_uint(ctx, -2);

	if (src_data != NULL && src_len >= 1 && src_data[0] == (char) 0xbf) {
		/* Bytecode. */
		void *buf;
		buf = duk_push_fixed_buffer(ctx, src_len);
		memcpy(buf, (const void *) src_data, src_len);
		duk_load_function(ctx);

	} else {
		/* Source code. */
		comp_flags = DUK_COMPILE_SHEBANG;
		duk_compile_lstring_filename(ctx, comp_flags, src_data, src_len);
	}

	/* [ ... bytecode_filename src_data src_len function ] */

	/* Optional bytecode dump. */
	if (duk_is_string(ctx, -4)) {
		FILE *f;
		void *bc_ptr;
		duk_size_t bc_len;
		size_t wrote;
		char fnbuf[256];
		const char *filename;

		duk_dup_top(ctx);
		duk_dump_function(ctx);
		bc_ptr = duk_require_buffer_data(ctx, -1, &bc_len);
		filename = duk_require_string(ctx, -5);

		snprintf(fnbuf, sizeof(fnbuf), "%s", filename);

		fnbuf[sizeof(fnbuf) - 1] = (char) 0;

		f = fopen(fnbuf, "wb");
		if (!f) {
			(void) duk_generic_error(ctx, "failed to open bytecode output file");
		}
		wrote = fwrite(bc_ptr, 1, (size_t) bc_len, f);  /* XXX: handle partial writes */
		(void) fclose(f);
		if (wrote != bc_len) {
			(void) duk_generic_error(ctx, "failed to write all bytecode");
		}

		return 0;  /* duk_safe_call() cleans up */
	}

	duk_push_global_object(ctx);  /* 'this' binding */
	duk_call_method(ctx, 0);

	return 0;  /* duk_safe_call() cleans up */
}


static int handle_eval(duk_context *ctx, const char *code) {
	int rc;
	int retval = -1;
	union {
		void *ptr;
		const void *constptr;
	} u;

	u.constptr = code;  /* Lose 'const' without warning. */
	duk_push_pointer(ctx, u.ptr);
	duk_push_uint(ctx, (duk_uint_t) strlen(code));
	duk_push_string(ctx, "eval");

	rc = duk_safe_call(ctx, wrapped_compile_execute, NULL /*udata*/, 3 /*nargs*/, 1 /*nret*/);

	if (rc != DUK_EXEC_SUCCESS) {
		return retval;
	} else {
		duk_pop(ctx);
		retval = 0;
	}

	return retval;
}

static int handle_fh(duk_context *ctx, FILE *f, const char *filename, const char *bytecode_filename) {
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

	duk_push_string(ctx, bytecode_filename);
	duk_push_pointer(ctx, (void *) buf);
	duk_push_uint(ctx, (duk_uint_t) bufoff);
	duk_push_string(ctx, filename);

	rc = duk_safe_call(ctx, wrapped_compile_execute, NULL /*udata*/, 4 /*nargs*/, 1 /*nret*/);

	free(buf);
	buf = NULL;

	if (rc != DUK_EXEC_SUCCESS) {

		if (buf) {
			free(buf);
			buf = NULL;
		}
		return retval;

	} else {
		duk_pop(ctx);
		retval = 0;
	}
	/* fall thru */

	if (buf) {
		free(buf);
		buf = NULL;
	}
	return retval;
}


static int handle_file(duk_context *ctx, const char *filename, const char *bytecode_filename) {
	FILE *f = NULL;
	int retval;
	char fnbuf[256];

	snprintf(fnbuf, sizeof(fnbuf), "%s", filename);

	fnbuf[sizeof(fnbuf) - 1] = (char) 0;

	f = fopen(fnbuf, "rb");
	if (!f) {
		fprintf(stderr, "failed to open source file: %s\n", filename);
		fflush(stderr);
		return -1;
	}

	retval = handle_fh(ctx, f, filename, bytecode_filename);

	fclose(f);
	return retval;
	
}

static duk_ret_t cb_resolve_module(duk_context *ctx) {
	const char *module_id;
	const char *parent_id;

	module_id = duk_require_string(ctx, 0);
	parent_id = duk_require_string(ctx, 1);

	duk_push_sprintf(ctx, "%s.js", module_id);
	printf("resolve_cb: id:'%s', parent-id:'%s', resolve-to:'%s'\n",
		module_id, parent_id, duk_get_string(ctx, -1));

	return 1;
}

int module_read(const char* path, char** data) {
    FILE* f;
    int fd;
    struct stat st;
    size_t fsize;

    *data = NULL;

    f = fopen(path, "rb");
    if (!f) {
        return -errno;
    }

    fd = fileno(f);
    assert(fd != -1);

    if (fstat(fd, &st) < 0) {
        fclose(f);
        return -errno;
    }

    if (S_ISDIR(st.st_mode)) {
        fclose(f);
        return -EISDIR;
    }

    fsize = st.st_size;

    *data = malloc(fsize);
    if (!*data) {
        fclose(f);
        return -ENOMEM;
    }

    fread(*data, 1, fsize, f);
    if (ferror(f)) {
        fclose(f);
        free(*data);
        return -EIO;
    }

    if (strncmp(*data, "#!", 2) == 0) {
        memcpy((void*) *data, "//", 2);
    }

    fclose(f);
	return 0;
}

int EndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix >  lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

static duk_ret_t cb_load_module(duk_context *ctx) {
    const char* resolved_id = duk_require_string(ctx, 0);

    if (EndsWith(resolved_id, ".js")) {
        char* data;
        int len;
        module_read(resolved_id, &data);
        if (len < 0) {
            return duk_generic_error(ctx, "Module could not be loaded: %s", resolved_id);
        }
        if (strncmp(data, "#!", 2) == 0) {
            memcpy((void*) data, "//", 2);
        }
        duk_push_string(ctx, data);
        free(data);
        return 1;
    }

	return 0;
}

static duk_ret_t athena_dofile(duk_context *ctx) {

	const char* errMsg;

	const char* script = duk_require_string(ctx, 0);

	if (handle_file(ctx, script, NULL) != 0) {
		errMsg = (const char*)malloc(strlen(duk_safe_to_stacktrace(ctx, -1)+1));
		sprintf((char*)errMsg, "\n%s", duk_safe_to_stacktrace(ctx, -1));
		return duk_generic_error(ctx, errMsg);
	}

	return 0;
}

static duk_ret_t athena_dostring(duk_context *ctx) {

	const char* errMsg;

	const char* script = duk_require_string(ctx, 0);

	if (handle_eval(ctx, script) != 0) {
		errMsg = (const char*)malloc(strlen(duk_safe_to_stacktrace(ctx, -1)));
		sprintf((char*)errMsg, "\n%s", duk_safe_to_stacktrace(ctx, -1));
		return duk_generic_error(ctx, errMsg);
	}

	return 0;
}

const char* runScript(const char* script, bool isBuffer)
{	

    const char* errMsg;

    printf("\nStarting AthenaEnv...\n");

  	ctx = duk_create_heap_default();

	duk_console_init(ctx, DUK_CONSOLE_PROXY_WRAPPER /*flags*/);

	duk_push_object(ctx);

	duk_push_c_function(ctx, cb_resolve_module, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "resolve");

	duk_push_c_function(ctx, cb_load_module, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "load");

	duk_push_c_function(ctx, athena_dofile, DUK_VARARGS);
	duk_put_global_string(ctx, "dofile");

	duk_push_c_function(ctx, athena_dostring, DUK_VARARGS);
	duk_put_global_string(ctx, "dostring");

	duk_module_node_init(ctx);

	athena_system_init(ctx);
	athena_render_init(ctx);
	athena_screen_init(ctx);
	athena_shape_init(ctx);
	athena_font_init(ctx);
	athena_image_init(ctx);
	athena_imagelist_init(ctx);
	athena_pads_init(ctx);
	athena_sound_init(ctx);
	athena_timer_init(ctx);
	athena_task_init(ctx);

    printf("AthenaEnv: top after init - %ld\n\n", (long) duk_get_top(ctx));

    // Run JavaScript
    if(!isBuffer){

		if (handle_file(ctx, script, NULL) != 0) {
			errMsg = (const char*)malloc(strlen(duk_safe_to_stacktrace(ctx, -1)));
	    	sprintf((char*)errMsg, "%s\n", duk_safe_to_stacktrace(ctx, -1));
	    }

    } else {
	    if (handle_eval(ctx, script) != 0) {
			errMsg = (const char*)malloc(strlen(duk_safe_to_stacktrace(ctx, -1)));
	    	sprintf((char*)errMsg, "%s\n", duk_safe_to_stacktrace(ctx, -1));
	    }
    }

	duk_destroy_heap(ctx);
	
	return errMsg;
}