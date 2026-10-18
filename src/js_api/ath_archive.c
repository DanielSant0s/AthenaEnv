#include <ath_env.h>
#include <athena/archive.h>
#include <macros.h>

static JSValue js_archiveopen(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	AthenaArchive *archive = athena_archive_open(path);
	JS_FreeCString(ctx, path);

	if (!archive)
		return JS_UNDEFINED;

	return JS_NewUint32(ctx, (uint32_t)(intptr_t)archive);
}

static JSValue js_archiveget(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	uint32_t ptr = 0;
	JS_ToUint32(ctx, &ptr, argv[0]);
	AthenaArchive *archive = (AthenaArchive *)(intptr_t)ptr;

	AthenaArchiveEntryListing *listing = athena_archive_list(archive);
	if (!listing)
		return JS_UNDEFINED;

	JSValue arr = JS_NewArray(ctx);
	for (int i = 0; i < listing->count; i++) {
		JSValue obj = JS_NewObject(ctx);
		JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, listing->entries[i].name), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewUint32(ctx, listing->entries[i].size), JS_PROP_C_W_E);
		JS_DefinePropertyValueStr(ctx, obj, "mtime", JS_NewUint32(ctx, listing->entries[i].mtime), JS_PROP_C_W_E);
		JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
	}

	athena_archive_entry_listing_free(listing);
	return arr;
}

static JSValue js_archiveclose(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	uint32_t ptr = 0;
	JS_ToUint32(ctx, &ptr, argv[0]);
	AthenaArchive *archive = (AthenaArchive *)(intptr_t)ptr;
	athena_archive_close(archive);
	return JS_UNDEFINED;
}

static JSValue js_untar(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	athena_archive_untar(path);
	JS_FreeCString(ctx, path);
	return JS_UNDEFINED;
}

static JSValue js_extractall(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	uint32_t ptr = 0;
	JS_ToUint32(ctx, &ptr, argv[0]);
	AthenaArchive *archive = (AthenaArchive *)(intptr_t)ptr;

	if (archive->type == ATHENA_ARCHIVE_GZ) {
		void *out = NULL;
		size_t out_size = 0;
		if (athena_archive_extract_all(archive, &out, &out_size) != 0)
			return JS_UNDEFINED;
		return JS_NewArrayBuffer(ctx, out, out_size, NULL, NULL, 1);
	}

	athena_archive_extract_all(archive, NULL, NULL);
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("open", 1, js_archiveopen),
	JS_CFUNC_DEF("list", 1, js_archiveget),
	JS_CFUNC_DEF("extractAll", 1, js_extractall),
	JS_CFUNC_DEF("close", 1, js_archiveclose),
	JS_CFUNC_DEF("untar", 1, js_untar),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
	return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_archive_init(JSContext *ctx)
{
	return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Archive");
}

#ifdef DYNAMIC_ATHENA_ARCHIVE
char *erl_id = "athena_archive";

static int erl_module_init(JSContext *ctx, JSModuleDef *m)
{
	if (JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs)) != 0)
		return -1;

	JSValue archive_obj = JS_NewObject(ctx);
	JS_SetPropertyFunctionList(ctx, archive_obj, module_funcs, countof(module_funcs));

	JSValue global_obj = JS_GetGlobalObject(ctx);
	JS_SetPropertyStr(ctx, global_obj, "Archive", JS_DupValue(ctx, archive_obj));
	JS_FreeValue(ctx, global_obj);

	JS_SetModuleExport(ctx, m, "Archive", archive_obj);
	return 0;
}

JSModuleDef *js_init_module(JSContext *ctx)
{
	JSModuleDef *m = athena_push_module(ctx, erl_module_init, module_funcs, countof(module_funcs), "Archive");
	if (m)
		JS_AddModuleExport(ctx, m, "Archive");
	return m;
}
#endif
