#include <unistd.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <elf-loader.h>
#include <libcdvd.h>
#include <timer.h>
#include <dbgprintf.h>

#include <ath_env.h>
#include <system.h>
#include <memory.h>
#include <def_mods.h>
#include <taskman.h>

#include <usbhdfsd-common.h>
#include <hdd-ioctl.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <io_common.h>

#include <iop_manager.h>

#include <macros.h>
#include <n32_call.h>

#include <athena/system_facade.h>
#include <erl.h>

#define MAX_DIR_FILES 512

static JSValue js_dir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    if (argc != 0 && argc != 1) return JS_ThrowSyntaxError(ctx, "Argument error: System.listDir([path]) takes zero or one argument.");

    const char *temp_path = NULL;
    if (argc != 0)
        temp_path = JS_ToCString(ctx, argv[0]);

    AthenaDirListing *listing = athena_system_list_dir(temp_path);
    if (temp_path)
        JS_FreeCString(ctx, temp_path);

    JSValue arr = JS_NewArray(ctx);
    if (!listing)
        return arr;

    for (int i = 0; i < listing->count; i++) {
        JSValue obj = JS_NewObject(ctx);
        JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, listing->entries[i].name), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewUint32(ctx, listing->entries[i].size), JS_PROP_C_W_E);
        JS_DefinePropertyValueStr(ctx, obj, "dir", JS_NewBool(ctx, listing->entries[i].is_dir), JS_PROP_C_W_E);
        JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
    }

    athena_system_dir_listing_free(listing);
    return arr;
}

static JSValue js_removeDir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if(!path) return JS_ThrowSyntaxError(ctx, "Argument error: System.removeDirectory(directory) takes a directory name as string as argument.");
	athena_system_remove_dir(path);
	JS_FreeCString(ctx, path);
	return JS_UNDEFINED;
}

static JSValue js_movefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *oldName = JS_ToCString(ctx, argv[0]);
	const char *newName = JS_ToCString(ctx, argv[1]);
	if(!oldName || !newName)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

	athena_system_move_file(oldName, newName);
	JS_FreeCString(ctx, oldName);
	JS_FreeCString(ctx, newName);
	return JS_UNDEFINED;
}

static JSValue js_rename(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *oldName = JS_ToCString(ctx, argv[0]);
	const char *newName = JS_ToCString(ctx, argv[1]);
	if(!oldName || !newName)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

	athena_system_move_file(oldName, newName);
	JS_FreeCString(ctx, oldName);
	JS_FreeCString(ctx, newName);
	return JS_UNDEFINED;
}

static JSValue js_copyfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *ogfile = JS_ToCString(ctx, argv[0]);
	const char *newfile = JS_ToCString(ctx, argv[1]);
	if(!ogfile || !newfile)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.copyFile(source, destination) takes two filenames as strings as arguments.");

	athena_system_copy_file(ogfile, newfile);
	JS_FreeCString(ctx, ogfile);
	JS_FreeCString(ctx, newfile);
	return JS_UNDEFINED;
}

static JSValue js_sleep(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "milliseconds expected.");
	int sec;
	JS_ToInt32(ctx, &sec, argv[0]);
	athena_system_sleep(sec);
	return JS_UNDEFINED;
}

static JSValue js_delay(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	nopdelay();
	return JS_UNDEFINED;
}

static JSValue js_exit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "System.exitToBrowser");
	athena_system_poweroff();
	return JS_UNDEFINED;
}


void recursive_mkdir(char *dir) {
	char *p = dir;
	while (p) {
		char *p2 = strstr(p, "/");
		if (p2) {
			p2[0] = 0;
			mkdir(dir, 0777);
			p = p2 + 1;
			p2[0] = '/';
		} else break;
	}
}

static JSValue js_getmcinfo(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int mcslot, type, freespace, format, result;
	mcslot = 0;
	if(argc == 1) JS_ToInt32(ctx, &mcslot, argv[0]);

	mcGetInfo(mcslot, 0, &type, &freespace, &format);
	mcSync(0, NULL, &result);

	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "type", JS_NewUint32(ctx, type), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "freemem", JS_NewUint32(ctx, freespace), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "format", JS_NewUint32(ctx, format), JS_PROP_C_W_E);

	return obj;
}

static JSValue js_loadELF(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
 	JSValue val;
	int n = 0;
	char **args = NULL;
	const char *path = JS_ToCString(ctx, argv[0]);
	bool reset_iop = false;

	if(argc > 1) {
		if (!JS_IsArray(ctx, argv[1])) {
		    return JS_ThrowSyntaxError(ctx, "Type error, you should use a string array.");
		}

		val = JS_GetPropertyStr(ctx, argv[1], "length");
		JS_ToInt32(ctx, &n, val);
		JS_FreeValue(ctx, val);
		args = malloc(n*sizeof(char*));

		for (int i = 0; i < n; i++) {
			val = JS_GetPropertyUint32(ctx, argv[1], i);
		    *(args + i) = (char*)JS_ToCString(ctx, val);
			JS_FreeValue(ctx, val);
		}
	}

	if (argc > 2)
		reset_iop = JS_ToBool(ctx, argv[2]) != 0;

	athena_system_load_elf(path, n, args, reset_iop);
	JS_FreeCString(ctx, path);

	return JS_UNDEFINED;
}

DiscType DiscTypes[] = {
    {SCECdGDTFUNCFAIL, "FAIL", -1},
	{SCECdNODISC, "!", 1},
    {SCECdDETCT, "??", 2},
    {SCECdDETCTCD, "CD ?", 3},
    {SCECdDETCTDVDS, "DVD-SL ?", 4},
    {SCECdDETCTDVDD, "DVD-DL ?", 5},
    {SCECdUNKNOWN, "Unknown", 6},
    {SCECdPSCD, "PS1 CD", 7},
    {SCECdPSCDDA, "PS1 CDDA", 8},
    {SCECdPS2CD, "PS2 CD", 9},
    {SCECdPS2CDDA, "PS2 CDDA", 10},
    {SCECdPS2DVD, "PS2 DVD", 11},
    {SCECdESRDVD_0, "ESR DVD (off)", 12},
    {SCECdESRDVD_1, "ESR DVD (on)", 13},
    {SCECdCDDA, "Audio CD", 14},
    {SCECdDVDV, "Video DVD", 15},
    {SCECdIllegalMedia, "Unsupported", 16},
    {0x00, "", 0x00}  //end of list
};              //ends DiscTypes array


static JSValue js_checkValidDisc(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	int testValid;
	int result;
	result = 0;
	testValid = sceCdGetDiskType();
	switch (testValid) {
		case SCECdPSCD:
		case SCECdPSCDDA:
		case SCECdPS2CD:
		case SCECdPS2CDDA:
		case SCECdPS2DVD:
		case SCECdESRDVD_0:
		case SCECdESRDVD_1:
		case SCECdCDDA:
		case SCECdDVDV:
		case SCECdDETCTCD:
		case SCECdDETCTDVDS:
		case SCECdDETCTDVDD:
			result = 1;
		case SCECdNODISC:
		case SCECdDETCT:
		case SCECdUNKNOWN:
		case SCECdIllegalMedia:
			result = 0;
	}
	dbgprintf("Valid Disc: %d\n",result);
	return JS_NewInt32(ctx, result);
}

static JSValue js_checkDiscTray(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	int result;
	if (sceCdStatus() == SCECdStatShellOpen){
		result = 1;
	} else {
		result = 0;
	}
	return JS_NewInt32(ctx, result);
}


static JSValue js_getDiscType(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    int discType;
    int iz;
    discType = sceCdGetDiskType();

    int DiscType_ix = 0;
        for (iz = 0; DiscTypes[iz].name[0]; iz++)
            if (DiscTypes[iz].type == discType)
                DiscType_ix = iz;
    dbgprintf("getDiscType: %d\n",DiscTypes[DiscType_ix].value);
    return JS_NewInt32(ctx, DiscTypes[DiscType_ix].value); //return the value itself to Lua stack
}

extern void *_gp;

static volatile off_t progress, max_progress;

struct pathMap {
	const char* in;
	const char* out;
};

static int copyThread(void* data)
{
	struct pathMap* paths = (struct pathMap*)data;

    char buffer[BUFSIZ];
    int in = open(paths->in, O_RDONLY, 0);
    int out = open(paths->out, O_WRONLY | O_CREAT | O_TRUNC, 644);

    // Get the input file size
	uint32_t size = lseek(in, 0, SEEK_END);
	lseek(in, 0, SEEK_SET);

    progress = 0;
    max_progress = size;

    ssize_t bytes_read;
    while((bytes_read = read(in, buffer, BUFSIZ)) > 0)
    {
        write(out, buffer, bytes_read);
        progress += bytes_read;
    }

    // copy is done, or an error occurred
    close(in);
    close(out);
	free(paths);
	exit_kill_task();
    return JS_UNDEFINED;
}


static JSValue js_copyasync(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	struct pathMap* copypaths = (struct pathMap*)malloc(sizeof(struct pathMap));

	copypaths->in = JS_ToCString(ctx, argv[0]);
	copypaths->out = JS_ToCString(ctx, argv[1]);
	int task = create_task("FileSystem: Copy", (void*)copyThread, 8192+1024, 100);
	init_task(task, (void*)copypaths);
	return JS_UNDEFINED;
}


static JSValue js_getfileprogress(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "current", JS_NewInt32(ctx, (int)progress), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "final", JS_NewInt32(ctx, (int)max_progress), JS_PROP_C_W_E);

	return obj;
}

static JSValue js_mount(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 2 && argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	int mode = 0;
	const char *mountpoint = JS_ToCString(ctx, argv[0]);
	const char *blockdev = JS_ToCString(ctx, argv[1]);

	if (argc == 3)
		JS_ToInt32(ctx, &mode, argv[2]);

	int ret = fileXioMount(mountpoint, blockdev, mode);

	JS_FreeCString(ctx, mountpoint);
	JS_FreeCString(ctx, blockdev);

	return JS_NewInt32(ctx, ret);
}

static JSValue js_umount(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	const char *device = JS_ToCString(ctx, argv[0]);

	int ret = fileXioUmount(device);

	JS_FreeCString(ctx, device);

	return JS_NewInt32(ctx, ret);
}

static JSValue js_devices(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	int i, devcnt;
	struct fileXioDevice DEV[FILEXIO_MAX_DEVICES];
	
	devcnt = fileXioGetDeviceList(DEV, FILEXIO_MAX_DEVICES);
	if (devcnt > 0) {
		JSValue arr = JS_NewArray(ctx);
		for(i = 0; i < devcnt; i++ )
		{
			JSValue obj = JS_NewObject(ctx);
			JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, DEV[i].name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "desc", JS_NewString(ctx, DEV[i].desc), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
		}

		return arr;
	}

	return JS_NULL;
}

// Gets BDM driver name via fileXio
static JSValue js_getbdminfo(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 1)
		return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	const char *mass = JS_ToCString(ctx, argv[0]);

	int fd = fileXioDopen(mass);
	if (fd >= 0) {
		char dev_name[10];
		int dev_index = (int)(mass[4]-'0');
		if (fileXioIoctl2(fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, dev_name, sizeof(dev_name) - 1) >= 0) {
			dev_name[sizeof(dev_name) - 1] = '\0'; // Null-terminate the string before mapping
	
			fileXioDclose(fd);
	
			JSValue data = JS_NewObject(ctx);
		
			JS_DefinePropertyValueStr(ctx, data, "name", JS_NewString(ctx, dev_name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, data, "index", JS_NewInt32(ctx, dev_index), JS_PROP_C_W_E);
		
			return data;
		}
	}

	return JS_UNDEFINED;
}


static JSValue js_darkmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	dark_mode = JS_ToBool(ctx, argv[0]);
	return JS_UNDEFINED;
}


#define GS_REG_CSR (volatile u64 *)0x12001000 // System Status

static JSValue js_getcpuinfo(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    unsigned short int revision;
    unsigned int value;

	JSValue data = JS_NewObject(ctx);

    revision                                       = GetCop0(15);

	JS_DefinePropertyValueStr(ctx, data, "implementation", JS_NewInt32(ctx, (u8)(revision >> 8)), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "revision", JS_NewInt32(ctx, (u8)(revision & 0xFF)), JS_PROP_C_W_E);

    asm("cfc1 %0, $0\n"
        : "=r"(revision)
        :);

	JS_DefinePropertyValueStr(ctx, data, "FPUimplementation", JS_NewInt32(ctx, (u8)(revision >> 8)), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "FPUrevision", JS_NewInt32(ctx, (u8)(revision & 0xFF)), JS_PROP_C_W_E);

    value                                             = GetCop0(16);

	JS_DefinePropertyValueStr(ctx, data, "ICacheSize", JS_NewUint32(ctx, (u8)(value >> 9 & 3)), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "DCacheSize", JS_NewUint32(ctx, (u8)(value >> 6 & 3)), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "RAMSize", JS_NewUint32(ctx, GetMemorySize()), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "BUSClock", JS_NewUint32(ctx, kBUSCLK), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "CPUClock", JS_NewUint32(ctx, kBUSCLK*2), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "MachineType", JS_NewUint32(ctx, MachineType()), JS_PROP_C_W_E);

    return data;
}

static JSValue js_getgpuinfo(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    unsigned short int revision;
    unsigned int value;

	JSValue data = JS_NewObject(ctx);

    revision                                          = (*GS_REG_CSR) >> 16;
	JS_DefinePropertyValueStr(ctx, data, "revision", JS_NewInt32(ctx, (u8)(revision & 0xFF)), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "id", JS_NewInt32(ctx, (u8)(revision >> 8)), JS_PROP_C_W_E);

    return data;
}

static JSValue js_geteememory(JSContext *ctx, JSValue this_val, int argc, JSValueConst * argv){
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "Wrong number of arguments");

	AthenaMemoryStats stats;
	athena_system_get_memory_stats(&stats);

	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "core", JS_NewUint32(ctx, stats.core), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "nativeStack", JS_NewUint32(ctx, stats.native_stack), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "allocs", JS_NewUint32(ctx, stats.allocs), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "used", JS_NewUint32(ctx, stats.used), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "compressedBytes", JS_NewUint32(ctx, stats.compressed_bytes), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "uncompressedBytes", JS_NewUint32(ctx, stats.uncompressed_bytes), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "savedBytes", JS_NewUint32(ctx, stats.saved_bytes), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "compressedTextures", JS_NewUint32(ctx, stats.compressed_textures), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "trackedTextures", JS_NewUint32(ctx, stats.tracked_textures), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "compressedMeshes", JS_NewUint32(ctx, stats.compressed_meshes), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "trackedMeshes", JS_NewUint32(ctx, stats.tracked_meshes), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "compressCount", JS_NewUint32(ctx, stats.compress_count), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "decompressCount", JS_NewUint32(ctx, stats.decompress_count), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "oomRescues", JS_NewUint32(ctx, stats.oom_rescues), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "compressionEnabled", JS_NewBool(ctx, stats.compression_enabled), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "frameBudgetUs", JS_NewUint32(ctx, stats.frame_budget_us), JS_PROP_C_W_E);

	return obj;
}

static JSValue js_set_mem_compression(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	AthenaMemCompressConfig cfg;
	athena_mem_compress_get_config(&cfg);

	if (argc >= 1) {
		if (JS_IsObject(argv[0])) {
			JSValue v;
			v = JS_GetPropertyStr(ctx, argv[0], "enabled");
			if (!JS_IsUndefined(v)) cfg.enabled = JS_ToBool(ctx, v);
			JS_FreeValue(ctx, v);

			v = JS_GetPropertyStr(ctx, argv[0], "frameBudgetUs");
			if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.frame_budget_us, v);
			JS_FreeValue(ctx, v);

			v = JS_GetPropertyStr(ctx, argv[0], "textureVramFrames");
			if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.texture_vram_frames, v);
			JS_FreeValue(ctx, v);

			v = JS_GetPropertyStr(ctx, argv[0], "textureIdleFrames");
			if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.texture_idle_frames, v);
			JS_FreeValue(ctx, v);

			v = JS_GetPropertyStr(ctx, argv[0], "meshIdleFrames");
			if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.mesh_idle_frames, v);
			JS_FreeValue(ctx, v);
		} else {
			cfg.enabled = JS_ToBool(ctx, argv[0]);
		}
	}

	if (argc >= 2 && JS_IsObject(argv[1])) {
		JSValue v;
		v = JS_GetPropertyStr(ctx, argv[1], "frameBudgetUs");
		if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.frame_budget_us, v);
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, argv[1], "textureVramFrames");
		if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.texture_vram_frames, v);
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, argv[1], "textureIdleFrames");
		if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.texture_idle_frames, v);
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, argv[1], "meshIdleFrames");
		if (!JS_IsUndefined(v)) JS_ToUint32(ctx, &cfg.mesh_idle_frames, v);
		JS_FreeValue(ctx, v);
	}

	athena_mem_compress_set_config(&cfg);

	JSValue obj = JS_NewObject(ctx);
	JS_DefinePropertyValueStr(ctx, obj, "enabled", JS_NewBool(ctx, cfg.enabled), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "frameBudgetUs", JS_NewUint32(ctx, cfg.frame_budget_us), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "textureVramFrames", JS_NewUint32(ctx, cfg.texture_vram_frames), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "textureIdleFrames", JS_NewUint32(ctx, cfg.texture_idle_frames), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "meshIdleFrames", JS_NewUint32(ctx, cfg.mesh_idle_frames), JS_PROP_C_W_E);
	return obj;
}

static JSValue js_compact_memory(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	uint32_t target_bytes = 0;
	if (argc >= 1 && !JS_IsUndefined(argv[0]))
		JS_ToUint32(ctx, &target_bytes, argv[0]);

	size_t freed = athena_mem_compact_now((size_t)target_bytes);
	return JS_NewUint32(ctx, (uint32_t)freed);
}

static void js_free_malloc_buf(JSRuntime *rt, void *opaque, void *ptr) {
	free(ptr);
}

static JSValue js_lz4_compress(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc < 1)
		return JS_ThrowSyntaxError(ctx, "Expected ArrayBuffer or TypedArray");

	size_t byte_offset = 0, byte_len = 0;
	JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &byte_len, NULL);
	uint8_t *src = NULL;
	size_t src_len = 0;

	if (!JS_IsException(ab)) {
		size_t total_len = 0;
		uint8_t *base = JS_GetArrayBuffer(ctx, &total_len, ab);
		JS_FreeValue(ctx, ab);
		if (base && byte_offset + byte_len <= total_len) {
			src = base + byte_offset;
			src_len = byte_len;
		}
	} else {
		JS_FreeValue(ctx, JS_GetException(ctx));
		src = JS_GetArrayBuffer(ctx, &src_len, argv[0]);
	}

	if (!src || src_len == 0)
		return JS_ThrowTypeError(ctx, "Invalid input buffer");

	size_t bound = athena_lz4_compress_bound(src_len);
	if (bound == 0)
		return JS_ThrowRangeError(ctx, "Buffer too large for LZ4");

	uint8_t *dst = (uint8_t *)malloc(bound);
	if (!dst)
		return JS_ThrowOutOfMemory(ctx);

	int csize = athena_lz4_compress(src, dst, (int)src_len, (int)bound);
	if (csize <= 0) {
		free(dst);
		return JS_NULL;
	}

	uint8_t *shrunk = (uint8_t *)realloc(dst, (size_t)csize);
	if (shrunk)
		dst = shrunk;

	return JS_NewArrayBuffer(ctx, dst, (size_t)csize, js_free_malloc_buf, NULL, false);
}

static JSValue js_lz4_decompress(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_ThrowSyntaxError(ctx, "Expected (compressedBuffer, uncompressedSize)");

	uint32_t uncompressed_size = 0;
	if (JS_ToUint32(ctx, &uncompressed_size, argv[1]) || uncompressed_size == 0)
		return JS_ThrowRangeError(ctx, "Invalid uncompressedSize");

	size_t byte_offset = 0, byte_len = 0;
	JSValue ab = JS_GetTypedArrayBuffer(ctx, argv[0], &byte_offset, &byte_len, NULL);
	uint8_t *src = NULL;
	size_t src_len = 0;

	if (!JS_IsException(ab)) {
		size_t total_len = 0;
		uint8_t *base = JS_GetArrayBuffer(ctx, &total_len, ab);
		JS_FreeValue(ctx, ab);
		if (base && byte_offset + byte_len <= total_len) {
			src = base + byte_offset;
			src_len = byte_len;
		}
	} else {
		JS_FreeValue(ctx, JS_GetException(ctx));
		src = JS_GetArrayBuffer(ctx, &src_len, argv[0]);
	}

	if (!src || src_len == 0)
		return JS_ThrowTypeError(ctx, "Invalid compressed buffer");

	uint8_t *dst = (uint8_t *)malloc(uncompressed_size);
	if (!dst)
		return JS_ThrowOutOfMemory(ctx);

	int dsize = athena_lz4_decompress(src, dst, (int)src_len, (int)uncompressed_size);
	if (dsize < 0) {
		free(dst);
		return JS_NULL;
	}

	return JS_NewArrayBuffer(ctx, dst, (size_t)dsize, js_free_malloc_buf, NULL, false);
}

static JSValue js_gettemps(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	// Based on PS2Ident libxcdvd from SP193
	unsigned char in_buffer[1], out_buffer[16];
	int result;
	int stat = 0;

    memset(&out_buffer, 0, 16);

	in_buffer[0]= 0xEF;
	if((result=sceCdApplySCmd(0x03, in_buffer, sizeof(in_buffer), out_buffer))!=0)
	{
		stat=out_buffer[0];
	}

	if( !stat) {
		unsigned short temp = out_buffer[1] * 256 + out_buffer[2];
		return JS_NewFloat32(ctx, (float)((temp - (temp%128) ) / 128.0f) + (float)((temp%128))/10.0f);
	}

	return JS_UNDEFINED;

}

static JSValue js_loaddynamiclibrary(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	struct erl_record_t *erl = _init_load_erl_from_file(JS_ToCString(ctx, argv[0]), 0);

	if (erl)
		return JS_NewUint32(ctx, erl);

	return JS_UNDEFINED;
}

static JSValue js_unloaddynamiclibrary(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	struct erl_record_t *erl = NULL;
	JS_ToUint32(ctx, &erl, argv[0]);

	if (erl)
		return JS_NewUint32(ctx, unload_erl(erl));

	return JS_UNDEFINED;
}

static JSValue js_findlocalrelocatableobject(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  	struct erl_record_t *erl = NULL;
	JS_ToUint32(ctx, &erl, argv[0]);

	if (erl) {
		struct symbol_t *symbol = erl_find_local_symbol(JS_ToCString(ctx, argv[1]), erl);
		if (symbol)
			return JS_NewUint32(ctx, symbol->address);
	}

	return JS_UNDEFINED;
}

static JSValue js_findrelocatableobject(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	struct symbol_t *symbol = erl_find_symbol(JS_ToCString(ctx, argv[0]));
	if (symbol)
		return JS_NewUint32(ctx, symbol->address);

	return JS_UNDEFINED;
}

static JSValue js_call_native(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	func_arg arg[32] = { 0 };
	uint32_t arg_count = 0, arg_type = 0, return_type = NONTYPE_VOID;

	void *func = NULL;

	JS_ToUint32(ctx, &func, argv[0]);

	JS_ToUint32(ctx, &arg_count, JS_GetPropertyStr(ctx, argv[1], "length"));

	if (argc > 2)
		JS_ToUint32(ctx, &return_type, argv[2]);

	for (uint32_t i = 0; i < arg_count; i++) {
		JSValue obj = JS_GetPropertyUint32(ctx, argv[1], i);
		JS_ToUint32(ctx, &arg_type, JS_GetPropertyStr(ctx, obj, "type"));
		JSValue val = JS_GetPropertyStr(ctx, obj, "value");

		switch (arg_type) {
			case TYPE_LONG:
			case TYPE_ULONG:
				JS_ToInt64(ctx, &arg[i].ulong_arg, val);
				break;
			case TYPE_INT:
			case TYPE_UINT:
			case TYPE_SHORT:
			case TYPE_USHORT:
			case TYPE_CHAR:
			case TYPE_UCHAR:
			case TYPE_PTR:
				JS_ToInt32(ctx, &arg[i].uint_arg, val);
				break;
			case TYPE_BOOL:
				arg[i].bool_arg = JS_ToBool(ctx, val);
				break;
			case TYPE_FLOAT:
				{
					float f;
					JS_ToFloat32(ctx, &f, val);
					process_float_arg(f, i);
				}
				break;
			case TYPE_STRING:
				arg[i].string_arg = JS_ToCString(ctx, val);
				break;
			case TYPE_BUFFER:
				{
					uint32_t tmp;
					arg[i].ptr_arg = JS_GetArrayBuffer(ctx, &tmp, val);
				}
				
				break;
		}
	}

	switch (return_type) {
		case NONTYPE_VOID:
			call_function(void, func)(
				arg[0],  arg[1],  arg[2],  arg[3], 
				arg[4],  arg[5],  arg[6],  arg[7], 
				arg[8],  arg[9],  arg[10], arg[11], 
				arg[12], arg[13], arg[14], arg[15], 
				arg[16], arg[17], arg[18], arg[19]
			);
			break;
		case TYPE_INT:
		case TYPE_UINT:
		case TYPE_SHORT:
		case TYPE_USHORT:
		case TYPE_CHAR:
		case TYPE_UCHAR:
		case TYPE_PTR:
			{
				int ret = call_function(int, func)(
					arg[0],  arg[1],  arg[2],  arg[3], 
					arg[4],  arg[5],  arg[6],  arg[7], 
					arg[8],  arg[9],  arg[10], arg[11], 
					arg[12], arg[13], arg[14], arg[15], 
					arg[16], arg[17], arg[18], arg[19]
				);

				return JS_NewInt32(ctx, ret);
			}
		case TYPE_BOOL:
			{
				bool ret = call_function(bool, func)(
					arg[0],  arg[1],  arg[2],  arg[3], 
					arg[4],  arg[5],  arg[6],  arg[7], 
					arg[8],  arg[9],  arg[10], arg[11], 
					arg[12], arg[13], arg[14], arg[15], 
					arg[16], arg[17], arg[18], arg[19]
				);

				return JS_NewBool(ctx, ret);
			}
		case TYPE_FLOAT:
			{
				float ret = call_function(float, func)(
					arg[0],  arg[1],  arg[2],  arg[3], 
					arg[4],  arg[5],  arg[6],  arg[7], 
					arg[8],  arg[9],  arg[10], arg[11], 
					arg[12], arg[13], arg[14], arg[15], 
					arg[16], arg[17], arg[18], arg[19]
				);

				return JS_NewFloat32(ctx, ret);
			}
		case TYPE_STRING:
			{
				char *ret = call_function(string, func)(
					arg[0],  arg[1],  arg[2],  arg[3], 
					arg[4],  arg[5],  arg[6],  arg[7], 
					arg[8],  arg[9],  arg[10], arg[11], 
					arg[12], arg[13], arg[14], arg[15], 
					arg[16], arg[17], arg[18], arg[19]
				);

				return JS_NewString(ctx, ret);
			}

	}

	for (uint32_t i = 0; i < arg_count; i++) {
		JSValue obj = JS_GetPropertyUint32(ctx, argv[1], i);
		JS_ToUint32(ctx, &arg_type, JS_GetPropertyStr(ctx, obj, "type"));
		JSValue val = JS_GetPropertyStr(ctx, obj, "value");

		JS_FreeValue(ctx, val);
		JS_FreeValue(ctx, val);
		
		JS_FreeValue(ctx, obj);
		JS_FreeValue(ctx, obj);
	}

	return JS_UNDEFINED;
}

static const JSCFunctionListEntry system_funcs[] = {
	JS_CFUNC_DEF( "getBDMInfo",               0,        js_getbdminfo		 	 ),
	JS_CFUNC_DEF( "mount",                    3,        js_mount		 		 ),
	JS_CFUNC_DEF( "umount",                   1,        js_umount		 		 ),
	JS_CFUNC_DEF( "devices",                  0,        js_devices			 	 ),
	JS_CFUNC_DEF( "listDir",                  1,        js_dir			 		 ),
	JS_CFUNC_DEF( "removeDirectory",    	  1,       	js_removeDir	 		 ),
	JS_CFUNC_DEF( "moveFile",	       		  2,        js_movefile	 			 ),
	JS_CFUNC_DEF( "copyFile",	       		  2,        js_copyfile	 			 ),
	JS_CFUNC_DEF( "threadCopyFile",	   		  2,       	js_copyasync	   	     ),
	JS_CFUNC_DEF( "getFileProgress",	  	  0,  		js_getfileprogress       ),
	JS_CFUNC_DEF( "rename",           		  2,        js_rename		 		 ),
	JS_CFUNC_DEF( "sleep",           		  1,        js_sleep		 		 ),
	JS_CFUNC_DEF( "delay",           		  0,        js_delay		 		 ),
	JS_CFUNC_DEF( "exitToBrowser",  		  0,        js_exit	 				 ),
	JS_CFUNC_DEF( "getMCInfo",                2,       	js_getmcinfo	 		 ),
	JS_CFUNC_DEF( "loadELF",                  3,        js_loadELF	 			 ),
	JS_CFUNC_DEF( "checkValidDisc",     	  0,  		js_checkValidDisc 		 ),
	JS_CFUNC_DEF( "getDiscType",        	  0,     	js_getDiscType	  		 ),
	JS_CFUNC_DEF( "checkDiscTray",      	  0,   		js_checkDiscTray	 	 ),
	JS_CFUNC_DEF( "setDarkMode",      		  1,   		js_darkmode	 			 ),
	JS_CFUNC_DEF( "getCPUInfo",      		  0,   		js_getcpuinfo	 		 ),
	JS_CFUNC_DEF( "getGPUInfo",      		  0,   		js_getgpuinfo	 		 ),
	JS_CFUNC_DEF( "getMemoryStats",      	  0,   		js_geteememory	 		 ),
	JS_CFUNC_DEF( "setMemoryCompression",     2,        js_set_mem_compression   ),
	JS_CFUNC_DEF( "compactMemory",            1,        js_compact_memory        ),
	JS_CFUNC_DEF( "lz4Compress",              1,        js_lz4_compress          ),
	JS_CFUNC_DEF( "lz4Decompress",            2,        js_lz4_decompress        ),
	JS_CFUNC_DEF( "getTemperature",      	  0,   		js_gettemps	 			 ),

	JS_CFUNC_DEF( "nativeCall",      	      3, 		js_call_native		 	 ),

	JS_PROP_INT32_DEF("T_LONG",    TYPE_LONG,   JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_ULONG",   TYPE_ULONG,  JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_INT",     TYPE_INT,    JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_UINT",    TYPE_UINT,   JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_SHORT",   TYPE_SHORT,  JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_USHORT",  TYPE_USHORT, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_CHAR",    TYPE_CHAR,   JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_UCHAR",   TYPE_UCHAR,  JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_BOOL",    TYPE_BOOL,   JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_FLOAT",   TYPE_FLOAT,  JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_PTR",     TYPE_PTR,    JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("T_STRING",  TYPE_STRING, JS_PROP_CONFIGURABLE ),

	JS_PROP_INT32_DEF("JS_BUFFER", TYPE_BUFFER, JS_PROP_CONFIGURABLE ),

	JS_CFUNC_DEF( "loadReloc",      	    1, 		  js_loaddynamiclibrary	 ),
	JS_CFUNC_DEF( "unloadReloc",            1, 		  js_unloaddynamiclibrary	 ),
	JS_CFUNC_DEF( "findRelocObject",        1,        js_findrelocatableobject	 ),
	JS_CFUNC_DEF( "findRelocLocalObject",   2,        js_findlocalrelocatableobject	 ),

	JS_PROP_STRING_DEF("boot_path", boot_path, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("READ_ONLY", 1, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("SELECT", 2, JS_PROP_CONFIGURABLE ),
};

static int system_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, system_funcs, countof(system_funcs));
}

JSModuleDef *athena_system_init(JSContext* ctx){
	return athena_push_module(ctx, system_init, system_funcs, countof(system_funcs), "System");
}

