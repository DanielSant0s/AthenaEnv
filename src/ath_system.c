#include <unistd.h>
#include <libmc.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <elf-loader.h>

#include "ath_env.h"
#include "include/graphics.h"

#include "include/system.h"

#define MAX_DIR_FILES 512

static JSValue athena_getCurrentDirectory(JSContext *ctx)
{
	char path[256];
	getcwd(path, 256);
	return JS_NewString(ctx, path);
}

static JSValue athena_setCurrentDirectory(JSContext *ctx, JSValueConst *argv)
{
        static char temp_path[256];
	const char *path = JS_ToCString(ctx, argv[0]);
	if(!path) return JS_ThrowSyntaxError(ctx, "Argument error: System.currentDirectory(file) takes a filename as string as argument.");

	athena_getCurrentDirectory(ctx);
	
	// let's do what the ps2sdk should do, 
	// some normalization... :)
	// if absolute path (contains [drive]:path/)
	if (strchr(path, ':'))
	{
	      strcpy(temp_path,path);
	}
	else // relative path
	{
	   // remove last directory ?
	   if(!strncmp(path, "..", 2))
	   {
	        getcwd(temp_path, 256);
	        if ((temp_path[strlen(temp_path)-1] != ':'))
	        {
	           int idx = strlen(temp_path)-1;
	           do
	           {
	                idx--;
	           } while (temp_path[idx] != '/');
	           temp_path[idx] = '\0';
	        }
	        
           }
           // add given directory to the existing path
           else
           {
	      getcwd(temp_path, 256);
	      strcat(temp_path,"/");
	      strcat(temp_path,path);
	   }
        }
        
        printf("changing directory to %s\n",__ps2_normalize_path(temp_path));
        chdir(__ps2_normalize_path(temp_path));
       
	return JS_UNDEFINED;
}

static JSValue athena_curdir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if(argc == 0) return athena_getCurrentDirectory(ctx);
	if(argc == 1) return athena_setCurrentDirectory(ctx, argv);
	return JS_ThrowSyntaxError(ctx, "Argument error: System.currentDirectory([file]) takes zero or one argument.");
}


static JSValue athena_dir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	
	if (argc != 0 && argc != 1) return JS_ThrowSyntaxError(ctx, "Argument error: System.listDir([path]) takes zero or one argument.");
	
	JSValue arr = JS_NewArray(ctx);

    const char *temp_path = "";
	char path[255];
	
	getcwd((char *)path, 256);
	printf("current dir %s\n",(char *)path);
	
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
	
	strcpy(path,__ps2_normalize_path(path));
	printf("\nchecking path : %s\n",path);
		

        
    //-----------------------------------------------------------------------------------------
	// read from MC ?
        
    if( !strcmp( path, "mc0:" ) || !strcmp( path, "mc1:" ) )
    {       
        int	nPort, numRead;
        char mcPath[256];
		sceMcTblGetDir mcEntries[MAX_DIR_FILES] __attribute__((aligned(64)));
		
		if( !strcmp( path, "mc0:" ) )
			nPort = 0;
		else
			nPort = 1;
		
		
		// copy only the path without the device (ie : mc0:/xxx/xxx -> /xxx/xxx)
		strcpy(mcPath,(char *)&path[4]);
				
		// it temp_path is empty put a "/" inside
        if (strlen(mcPath)==0)
           strcpy((char *)mcPath,(char *)"/");
		

		if (mcPath[strlen(mcPath)-1] != '/')
		  strcat( mcPath, "/-*" );
		else
		  strcat( mcPath, "*" );
	
		mcGetDir( nPort, 0, mcPath, 0, MAX_DIR_FILES, mcEntries);
   		while (!mcSync( MC_WAIT, NULL, &numRead ));
   		                	    
	    int cpt = 0;

		for( int i = 0; i < numRead; i++ )
		{	
			JSValue obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, (const char *)mcEntries[i].EntryName), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewUint32(ctx, mcEntries[i].FileSizeByte), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "dir", JS_NewBool(ctx, ( mcEntries[i].AttrFile & MC_ATTR_SUBDIR )), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, cpt++, obj, JS_PROP_C_W_E);

		}
		return arr; 
    }
    //-----------------------------------------------------------------------------------------
    
    // else regular one using opendir/readdir

	int i = 0;

	DIR *d;
	struct dirent *dir;
	d = opendir(path);

	if (d) {
		while ((dir = readdir(d)) != NULL) {

			JSValue obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, dir->d_name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewUint32(ctx, dir->d_stat.st_size), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "dir", JS_NewBool(ctx, S_ISDIR(dir->d_stat.st_mode)), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i++, obj, JS_PROP_C_W_E);
	    }
	    closedir(d);
	}
	
	return arr;
}

static JSValue athena_createDir(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if(!path) return JS_ThrowSyntaxError(ctx, "Argument error: System.createDirectory(directory) takes a directory name as string as argument.");
	mkdir(path, 0777);
	
	return JS_UNDEFINED;
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

static JSValue athena_removeFile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	const char *path = JS_ToCString(ctx, argv[0]);
	if(!path) return JS_ThrowSyntaxError(ctx, "Argument error: System.removeFile(filename) takes a filename as string as argument.");
	remove(path);

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

static JSValue athena_getFreeMemory(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 0) return JS_ThrowSyntaxError(ctx, "no arguments expected.");
	
	size_t result = GetFreeSize();

	return JS_NewUint32(ctx, (uint32_t)(result));
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

static JSValue athena_openfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	const char *file_tbo = JS_ToCString(ctx, argv[0]);
	int type;
	JS_ToInt32(ctx, &type, argv[1]);

	int fd = open(file_tbo, type, 0777);
	if (fd < 0) return JS_ThrowSyntaxError(ctx, "cannot open requested file.");

	return JS_NewInt32(ctx, fd);
}


static JSValue athena_readfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 2) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	
	int file;
	uint32_t size;

	JS_ToInt32(ctx, &file, argv[0]);
	JS_ToUint32(ctx, &size, argv[1]);
	uint8_t *buffer = (uint8_t*)malloc(size + 1);
	int len = read(file,buffer, size);
	buffer[len] = 0;
	return JS_NewStringLen(ctx, (const char*)buffer, len);
}


static JSValue athena_writefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");

	int fd;
	uint32_t size;

	JS_ToInt32(ctx, &fd, argv[0]);
	const char *text = JS_ToCString(ctx, argv[1]);
	JS_ToUint32(ctx, &size, argv[2]);
	write(fd, text, size);
	return JS_UNDEFINED;
}

static JSValue athena_closefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int fd;
	JS_ToInt32(ctx, &fd, argv[0]);
	close(fd);
	return JS_UNDEFINED;
}

static JSValue athena_seekfile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 3) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int fd;
	int pos;
	uint32_t type;

	JS_ToInt32(ctx, &fd, argv[0]);
	JS_ToInt32(ctx, &pos, argv[1]);
	JS_ToUint32(ctx, &type, argv[2]);

	lseek(fd, pos, type);	
	return JS_UNDEFINED;
}


static JSValue athena_sizefile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	int fd;
	JS_ToInt32(ctx, &fd, argv[0]);
	uint32_t cur_off = lseek(fd, 0, SEEK_CUR);
	uint32_t size = lseek(fd, 0, SEEK_END);
	lseek(fd, cur_off, SEEK_SET);
	return JS_NewUint32(ctx, size);
}


static JSValue athena_checkexist(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	if (argc != 1) return JS_ThrowSyntaxError(ctx, "wrong number of arguments");
	const char *file_tbo = JS_ToCString(ctx, argv[0]);
	int fd = open(file_tbo, O_RDONLY, 0777);
	if (fd < 0) return JS_NewBool(ctx, false);
	close(fd);
	return JS_NewBool(ctx, true);
}


static JSValue athena_loadELF(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv)
{
	if (argc != 1 && argc != 2) return JS_ThrowSyntaxError(ctx, "Argument error: System.loadELF() takes a string as argument.");

	JSValue val;
	int n = 0;
	char **args = NULL;
	const char *elftoload = JS_ToCString(ctx, argv[0]);

	if(argc == 2){
		if (!JS_IsArray(ctx, argv[1])) {
		    return JS_ThrowSyntaxError(ctx, "Type error, you should use a string array.");
		}

		val = JS_GetPropertyStr(ctx, argv[1], "length");
		JS_ToInt32(ctx, &n, val);

		args = malloc(n*sizeof(char*));

		for (int i = 0; i < n; i++) {
			val = JS_GetPropertyUint32(ctx, argv[1], i);
		    *(args + i) = (char*)JS_ToCString(ctx, val);
		}
	}

	LoadELFFromFile(elftoload, n, args);
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
	printf("Valid Disc: %d\n",result);
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
    printf("getDiscType: %d\n",DiscTypes[DiscType_ix].value);
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

static const JSCFunctionListEntry system_funcs[] = {
	JS_CFUNC_DEF( "openFile",           		  2,         athena_openfile),
	JS_CFUNC_DEF( "readFile",          		  2,         athena_readfile		 ),
	JS_CFUNC_DEF( "writeFile",          		  3,       	athena_writefile		 ),
	JS_CFUNC_DEF( "closeFile",          		  1,       	athena_closefile		 ),  
	JS_CFUNC_DEF( "seekFile",          		  3,         athena_seekfile		 ),  
	JS_CFUNC_DEF( "sizeFile",          		  1,         athena_sizefile			 ),
	JS_CFUNC_DEF( "doesFileExist",      		  1,      	athena_checkexist	 ),
	JS_CFUNC_DEF( "currentDir",   1,        		athena_curdir		 ),
	JS_CFUNC_DEF( "listDir",  1,         			athena_dir			 		 ),
	JS_CFUNC_DEF( "createDirectory",    		  1,       	athena_createDir	 ),
	JS_CFUNC_DEF( "removeDirectory",    		  1,       	athena_removeDir	 ),
	JS_CFUNC_DEF( "moveFile",	       		  2,        	athena_movefile	 ),
	JS_CFUNC_DEF( "copyFile",	       		  2,        	athena_copyfile	 ),
	JS_CFUNC_DEF( "threadCopyFile",	   		  2,       	athena_copyasync	   	 ),
	JS_CFUNC_DEF( "getFileProgress",	  		  0,  	athena_getfileprogress     ),
	JS_CFUNC_DEF( "removeFile",         		  2,      	athena_removeFile		 ),
	JS_CFUNC_DEF( "rename",           		  2,          athena_rename		 ),
	JS_CFUNC_DEF( "sleep",           		  1,           athena_sleep		 ),
	JS_CFUNC_DEF( "getFreeMemory",      		  0,   		athena_getFreeMemory ),
	JS_CFUNC_DEF( "exitToBrowser",  		  0,            athena_exit	 ),
	JS_CFUNC_DEF( "getMCInfo",          2,       	athena_getmcinfo	 		 ),
	JS_CFUNC_DEF( "loadELF",         3,        	athena_loadELF	 			 ),
	JS_CFUNC_DEF( "checkValidDisc",     		  0,  		athena_checkValidDisc ),
	JS_CFUNC_DEF( "getDiscType",        		  0,     	athena_getDiscType	 ),
	JS_CFUNC_DEF( "checkDiscTray",      		  0,   		athena_checkDiscTray	 ),
	JS_PROP_INT32_DEF("FREAD", O_RDONLY, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("FWRITE", O_WRONLY, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("FCREATE", O_CREAT | O_WRONLY, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("FRDWR", O_RDWR, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("SET", SEEK_SET, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("END", SEEK_END, JS_PROP_CONFIGURABLE ),
	JS_PROP_INT32_DEF("CUR", SEEK_CUR, JS_PROP_CONFIGURABLE ),
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

static const JSCFunctionListEntry sif_funcs[] = {
	JS_CFUNC_DEF("loadModule",            3, 		 athena_sifloadmodule),
	JS_CFUNC_DEF("loadModuleBuffer",      3,       athena_sifloadmodulebuffer),
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
	return athena_push_module(ctx, sif_init, sif_funcs, countof(sif_funcs), "Sif");
}

