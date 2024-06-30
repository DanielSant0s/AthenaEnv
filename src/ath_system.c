#include <unistd.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <elf-loader.h>
#include <libcdvd.h>
#include <timer.h>
#include <debug.h>

#include "ath_env.h"
#include "include/system.h"
#include "include/memory.h"
#include "include/def_mods.h"
#include "include/taskman.h"

#define MAX_DIR_FILES 512

static JSValue athena_dir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	
	if (argc != 0 && argc != 1) return JS_ThrowSyntaxError(ctx, "Argument error: System.listDir([path]) takes zero or one argument.");
	
	JSValue arr = JS_NewArray(ctx);

    const char *temp_path = "";
	char path[255], tpath[384];
	
	getcwd((char *)path, 256);
	dbgprintf("current dir %s\n",(char *)path);
	
	if (argc != 0) 
	{
		temp_path = JS_ToCString(ctx, argv[0]);
		// append the given path to the boot_path
	        
	        strcpy ((char *)path, boot_path);
	        
	        if (strchr(temp_path, ':'))
	           // workaround in case of temp_path is containing 
	           // a device name again
	           strcpy ((char *)path, temp_path);
	        else
	           strcat ((char *)path, temp_path);
	}
	
	//strcpy(path, __ps2_normalize_path(path));
	dbgprintf("\nchecking path : %s\n",path);

	int i = 0;

	DIR *d;
	struct dirent *dir;

	d = opendir(path);

	struct stat     statbuf;

	if (d) {
		while ((dir = readdir(d)) != NULL) {

			strcpy(tpath, path);
			strcat(tpath, "/");
			strcat(tpath, dir->d_name);
    		stat(tpath, &statbuf);

			JSValue obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, dir->d_name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewUint32(ctx, statbuf.st_size), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "dir", JS_NewBool(ctx, (dir->d_type == DT_DIR)), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i++, obj, JS_PROP_C_W_E);
	    }
	    closedir(d);
	}
	
	return arr;
}

static JSValue athena_removeDir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if(!path) return JS_ThrowSyntaxError(ctx, "Argument error: System.removeDirectory(directory) takes a directory name as string as argument.");
	rmdir(path);
	
	return JS_UNDEFINED;
}

static JSValue athena_movefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if(!path) return JS_ThrowSyntaxError(ctx, "Argument error: System.removeFile(filename) takes a filename as string as argument.");
		const char *oldName = JS_ToCString(ctx, argv[0]);
	const char *newName = JS_ToCString(ctx, argv[1]);
	if(!oldName || !newName)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

	char buf[BUFSIZ];
    size_t size;

	int source = open(oldName, O_RDONLY, 0);
    int dest = open(newName, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	while ((size = read(source, buf, BUFSIZ)) > 0) {
	   write(dest, buf, size);
    }

    close(source);
    close(dest);

	remove(oldName);

	return JS_UNDEFINED;
}

static JSValue athena_rename(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *oldName = JS_ToCString(ctx, argv[0]);
	const char *newName = JS_ToCString(ctx, argv[1]);
	if(!oldName || !newName)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

	char buf[BUFSIZ];
    size_t size;

	int source = open(oldName, O_RDONLY, 0);
    int dest = open(newName, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	while ((size = read(source, buf, BUFSIZ)) > 0) {
	   write(dest, buf, size);
    }

    close(source);
    close(dest);

	remove(oldName);
	
	return JS_UNDEFINED;
}

static JSValue athena_copyfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *ogfile = JS_ToCString(ctx, argv[0]);
	const char *newfile = JS_ToCString(ctx, argv[1]);
	if(!ogfile || !newfile)
		return JS_ThrowSyntaxError(ctx, "Argument error: System.copyFile(source, destination) takes two filenames as strings as arguments.");

	char buf[BUFSIZ];
    size_t size;

	int source = open(ogfile, O_RDONLY, 0);
    int dest = open(newfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	while ((size = read(source, buf, BUFSIZ)) > 0) {
	   write(dest, buf, size);
    }

    close(source);
    close(dest);
	
	return JS_UNDEFINED;
}

static char modulePath[256];

static void setModulePath()
{
	getcwd( modulePath, 256 );
}

static JSValue athena_sleep(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "milliseconds expected.");
	int sec;
	JS_ToInt32(ctx, &sec, argv[0]);
	sleep(sec);
	return JS_UNDEFINED;
}

static JSValue athena_delay(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	nopdelay();
	return JS_UNDEFINED;
}

static JSValue athena_exit(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "System.exitToBrowser");
	asm volatile(
            "li $3, 0x04;"
            "syscall;"
            "nop;"
        );
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

static JSValue athena_getmcinfo(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
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

static JSValue athena_loadELF(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{

	JSValue val;
	int n = 0;
	char **args = NULL;
	const char *path = JS_ToCString(ctx, argv[0]);

	if(argc == 2){
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

	LoadELFFromFile(path, n, args);
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


static JSValue athena_checkValidDisc(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
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

static JSValue athena_checkDiscTray(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	int result;
	if (sceCdStatus() == SCECdStatShellOpen){
		result = 1;
	} else {
		result = 0;
	}
	return JS_NewInt32(ctx, result);
}


static JSValue athena_getDiscType(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
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
	exitkill_task();
    return JS_UNDEFINED;
}


static JSValue athena_copyasync(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	struct pathMap* copypaths = (struct pathMap*)malloc(sizeof(struct pathMap));

	copypaths->in = JS_ToCString(ctx, argv[0]);
	copypaths->out = JS_ToCString(ctx, argv[1]);
	int task = create_task("FileSystem: Copy", (void*)copyThread, 8192+1024, 100);
	init_task(task, (void*)copypaths);
	return JS_UNDEFINED;
}


static JSValue athena_getfileprogress(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "current", JS_NewInt32(ctx, (int)progress), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "final", JS_NewInt32(ctx, (int)max_progress), JS_PROP_C_W_E);

	return obj;
}

static JSValue athena_darkmode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	dark_mode = JS_ToBool(ctx, argv[0]);
	return JS_UNDEFINED;
}


#define GS_REG_CSR (volatile u64 *)0x12001000 // System Status

static JSValue athena_getcpuinfo(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
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

static JSValue athena_getgpuinfo(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
    unsigned short int revision;
    unsigned int value;

	JSValue data = JS_NewObject(ctx);

    revision                                          = (*GS_REG_CSR) >> 16;
	JS_DefinePropertyValueStr(ctx, data, "revision", JS_NewInt32(ctx, (u8)(revision & 0xFF)), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, data, "id", JS_NewInt32(ctx, (u8)(revision >> 8)), JS_PROP_C_W_E);

    return data;
}

static JSValue athena_geteememory(JSContext *ctx, JSValue this_val, int argc, JSValueConst * argv){
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "Wrong number of arguments");

	JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "core", JS_NewUint32(ctx, get_binary_size()), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "nativeStack", JS_NewUint32(ctx, get_stack_size()), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "allocs", JS_NewUint32(ctx, get_allocs_size()), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "used", JS_NewUint32(ctx, get_used_memory()), JS_PROP_C_W_E);


	return obj;
}

static JSValue athena_gettemps(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
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

static JSValue athena_stacktrace(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	int n = 0;
	JS_ToInt32(ctx, &n, argv[0]);

	JSValue arr = JS_NewArray(ctx);

	unsigned int stackTrace[n];

	ps2GetStackTrace(stackTrace, n);

  	for(int i = 0; i < n; i++)
  	{
  		if (stackTrace[i] == 0)
  	  		break;

		JS_SetPropertyUint32(ctx, arr, i, JS_NewUint32(ctx, stackTrace[i]));
  	}

	return arr;
}

static const JSCFunctionListEntry system_funcs[] = {
	JS_CFUNC_DEF( "listDir",  1,         			athena_dir			 		 ),
	JS_CFUNC_DEF( "removeDirectory",    		  1,       	athena_removeDir	 ),
	JS_CFUNC_DEF( "moveFile",	       		  2,        	athena_movefile	 ),
	JS_CFUNC_DEF( "copyFile",	       		  2,        	athena_copyfile	 ),
	JS_CFUNC_DEF( "threadCopyFile",	   		  2,       	athena_copyasync	   	 ),
	JS_CFUNC_DEF( "getFileProgress",	  		  0,  	athena_getfileprogress     ),
	JS_CFUNC_DEF( "rename",           		  2,          athena_rename		 ),
	JS_CFUNC_DEF( "sleep",           		  1,           athena_sleep		 ),
	JS_CFUNC_DEF( "delay",           		  0,           athena_delay		 ),
	JS_CFUNC_DEF( "exitToBrowser",  		  0,            athena_exit	 ),
	JS_CFUNC_DEF( "getMCInfo",          2,       	athena_getmcinfo	 		 ),
	JS_CFUNC_DEF( "loadELF",         3,        	athena_loadELF	 			 ),
	JS_CFUNC_DEF( "checkValidDisc",     		  0,  		athena_checkValidDisc ),
	JS_CFUNC_DEF( "getDiscType",        		  0,     	athena_getDiscType	 ),
	JS_CFUNC_DEF( "checkDiscTray",      		  0,   		athena_checkDiscTray	 ),
	JS_CFUNC_DEF( "setDarkMode",      		  1,   		athena_darkmode	 ),
	JS_CFUNC_DEF( "getCPUInfo",      		  0,   		athena_getcpuinfo	 ),
	JS_CFUNC_DEF( "getGPUInfo",      		  0,   		athena_getgpuinfo	 ),
	JS_CFUNC_DEF( "getMemoryStats",      	  0,   		athena_geteememory	 ),
	JS_CFUNC_DEF( "getTemperature",      	  0,   		athena_gettemps	 ),
	JS_CFUNC_DEF( "getStackTrace",      	  1,   		athena_stacktrace	 ),
	JS_PROP_STRING_DEF("boot_path", boot_path, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("READ_ONLY", 1, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("SELECT", 2, JS_PROP_CONFIGURABLE ),
};

static JSValue athena_sifloadmodule(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1 && argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	const char *path = JS_ToCString(ctx, argv[0]);

	int arg_len = 0;
	const char *args = NULL;

	if(argc == 3){
		JS_ToInt32(ctx, &arg_len, argv[1]);
		args = JS_ToCString(ctx, argv[2]);
	}
	
	int result = SifLoadModule(path, arg_len, args);
	return JS_NewInt32(ctx, result);
}


static JSValue athena_sifloadmodulebuffer(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1 && argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	size_t size = 0;
	void* ptr = JS_ToCStringLen(ctx, &size, argv[0]);

	int arg_len = 0;
	const char *args = NULL;

	if(argc == 3){
		JS_ToInt32(ctx, &arg_len, argv[1]);
		args = JS_ToCString(ctx, argv[2]);
	}

	int result = SifExecModuleBuffer((void*)ptr, size, arg_len, args, NULL);
	return JS_NewInt32(ctx, result);
}

static JSValue athena_sifloaddefaultmodule(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int mod_id = -1;

	JS_ToInt32(ctx, &mod_id, argv[0]);

	if (mod_id == BOOT_MODULE) {
		mod_id = get_boot_device(boot_path);
	}

	load_default_module(mod_id);

	return JS_UNDEFINED;
}

static JSValue athena_resetiop(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	if(ds34bt_started) ds34bt_deinit();
	if(ds34usb_started) ds34usb_deinit();
	if(pads_started) padEnd();
	#ifdef ATHENA_AUDIO
	if(audio_started) audsrv_quit();
	#endif

	prepare_IOP();

	SifExecModuleBuffer(&poweroff_irx, size_poweroff_irx, 0, NULL, NULL);
	load_default_module(FILEXIO_MODULE);

	//poweroffSetCallback(&poweroffHandler, NULL);
	return JS_UNDEFINED;
}

static JSValue athena_getiopmemory(JSContext *ctx, JSValue this_val, int argc, JSValueConst * argv){
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "Wrong number of arguments");
	s32 freeram = 0;
    s32 usedram = 0;

    smem_read((void *)(1 * 1024 * 1024), &freeram, 4);
    usedram = (2 * 1024 * 1024) - freeram;

	JSValue obj = JS_NewObject(ctx);
    JS_DefinePropertyValueStr(ctx, obj, "free", JS_NewUint32(ctx, freeram), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "used", JS_NewUint32(ctx, usedram), JS_PROP_C_W_E);

	return obj;
}

static const JSCFunctionListEntry sif_funcs[] = {
	JS_CFUNC_DEF("loadModule",            3, 		 athena_sifloadmodule),
	JS_CFUNC_DEF("loadModuleBuffer",      3,       athena_sifloadmodulebuffer),
	JS_CFUNC_DEF("loadDefaultModule",     3,       athena_sifloaddefaultmodule),
	JS_CFUNC_DEF("reset",      			  0,     	athena_resetiop),
	JS_CFUNC_DEF("getMemoryStats",        0,     	athena_getiopmemory),
	JS_PROP_INT32_DEF("keyboard", KEYBOARD_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("mouse", MOUSE_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("freeram", FREERAM_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ds34bt", DS34BT_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ds34usb", DS34USB_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("network", NETWORK_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("pads", PADS_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("memcard", MC_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("audio", AUDIO_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("usb_mass", USB_MASS_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("cdfs", CDFS_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("hdd", HDD_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("boot_device", BOOT_MODULE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("camera", CAMERA_MODULE, JS_PROP_CONFIGURABLE),
};

static int system_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, system_funcs, countof(system_funcs));
}

static int sif_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, sif_funcs, countof(sif_funcs));
}

JSModuleDef *athena_system_init(JSContext* ctx){
	setModulePath();
	athena_push_module(ctx, system_init, system_funcs, countof(system_funcs), "System");
	return athena_push_module(ctx, sif_init, sif_funcs, countof(sif_funcs), "IOP");
}

