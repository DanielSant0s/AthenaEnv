#include <unistd.h>
#include <libmc.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include "ath_env.h"
#include "include/graphics.h"

#include "include/system.h"

#define MAX_DIR_FILES 512

duk_ret_t athena_getCurrentDirectory(duk_context *ctx)
{
	char path[256];
	getcwd(path, 256);
	duk_push_string(ctx, path);
	
	return 1;
}

duk_ret_t athena_setCurrentDirectory(duk_context *ctx)
{
        static char temp_path[256];
	const char *path = duk_get_string(ctx, 0);
	if(!path) return duk_generic_error(ctx, "Argument error: System.currentDirectory(file) takes a filename as string as argument.");

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
       
	return 1;
}

duk_ret_t athena_curdir(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if(argc == 0) return athena_getCurrentDirectory(ctx);
	if(argc == 1) return athena_setCurrentDirectory(ctx);
	return duk_generic_error(ctx, "Argument error: System.currentDirectory([file]) takes zero or one argument.");
}


duk_ret_t athena_dir(duk_context *ctx)
{
	
	int argc = duk_get_top(ctx);
	if (argc != 0 && argc != 1) return duk_generic_error(ctx, "Argument error: System.listDirectory([path]) takes zero or one argument.");
	
	duk_idx_t arr_idx = duk_push_array(ctx);

    const char *temp_path = "";
	char path[255];
	
	getcwd((char *)path, 256);
	printf("current dir %s\n",(char *)path);
	
	if (argc != 0) 
	{
		temp_path = duk_get_string(ctx, 0);
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
                int	nPort;
                int	numRead;
                char    mcPath[256];
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
			duk_idx_t obj_idx = duk_push_object(ctx);

            duk_push_string(ctx, (const char *)mcEntries[i].EntryName);
			duk_put_prop_string(ctx, obj_idx, "name");
        
            duk_push_number(ctx, mcEntries[i].FileSizeByte);
			duk_put_prop_string(ctx, obj_idx, "size");

            duk_push_boolean(ctx, ( mcEntries[i].AttrFile & MC_ATTR_SUBDIR ));
			duk_put_prop_string(ctx, obj_idx, "dir");

			duk_put_prop_index(ctx, arr_idx, cpt++);

		}
		return 1;  // table is already on top
        }
        //-----------------------------------------------------------------------------------------
        
        // else regular one using Dopen/Dread

	int i = 0;

	DIR *d;
	struct dirent *dir;
	d = opendir(path);

	if (d) {
		while ((dir = readdir(d)) != NULL) {

			duk_idx_t obj_idx = duk_push_object(ctx);

	    	printf("%s\n", dir->d_name);
			duk_push_string(ctx, dir->d_name);
			duk_put_prop_string(ctx, obj_idx, "name");
	
        	duk_push_number(ctx, dir->d_stat.st_size);
			duk_put_prop_string(ctx, obj_idx, "size");
        
			duk_push_boolean(ctx,  S_ISDIR(dir->d_stat.st_mode));
        	duk_put_prop_string(ctx, obj_idx, "dir");

			duk_put_prop_index(ctx, arr_idx, i++);
	    }
	    closedir(d);
	}
	else
	{
		duk_push_null(ctx);  // return null
		return 1;
	}
	
	return 1;  /* table is already on top */
}

duk_ret_t athena_createDir(duk_context *ctx)
{
	const char *path = duk_get_string(ctx, 0);
	if(!path) return duk_generic_error(ctx, "Argument error: System.createDirectory(directory) takes a directory name as string as argument.");
	mkdir(path, 0777);
	
	return 0;
}

duk_ret_t athena_removeDir(duk_context *ctx)
{
	const char *path = duk_get_string(ctx, 0);
	if(!path) return duk_generic_error(ctx, "Argument error: System.removeDirectory(directory) takes a directory name as string as argument.");
	rmdir(path);
	
	return 0;
}

duk_ret_t athena_movefile(duk_context *ctx)
{
	const char *path = duk_get_string(ctx, 0);
	if(!path) return duk_generic_error(ctx, "Argument error: System.removeFile(filename) takes a filename as string as argument.");
		const char *oldName = duk_get_string(ctx, 0);
	const char *newName = duk_get_string(ctx, 1);
	if(!oldName || !newName)
		return duk_generic_error(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

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

	return 0;
}

duk_ret_t athena_removeFile(duk_context *ctx)
{
	const char *path = duk_get_string(ctx, 0);
	if(!path) return duk_generic_error(ctx, "Argument error: System.removeFile(filename) takes a filename as string as argument.");
	remove(path);

	return 0;
}

duk_ret_t athena_rename(duk_context *ctx)
{
	const char *oldName = duk_get_string(ctx, 0);
	const char *newName = duk_get_string(ctx, 1);
	if(!oldName || !newName)
		return duk_generic_error(ctx, "Argument error: System.rename(source, destination) takes two filenames as strings as arguments.");

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
	
	return 0;
}

duk_ret_t athena_copyfile(duk_context *ctx)
{
	const char *ogfile = duk_get_string(ctx, 0);
	const char *newfile = duk_get_string(ctx, 1);
	if(!ogfile || !newfile)
		return duk_generic_error(ctx, "Argument error: System.copyFile(source, destination) takes two filenames as strings as arguments.");

	char buf[BUFSIZ];
    size_t size;

	int source = open(ogfile, O_RDONLY, 0);
    int dest = open(newfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	while ((size = read(source, buf, BUFSIZ)) > 0) {
	   write(dest, buf, size);
    }

    close(source);
    close(dest);
	
	return 0;
}

static char modulePath[256];

static void setModulePath()
{
	getcwd( modulePath, 256 );
}

duk_ret_t athena_sleep(duk_context *ctx)
{
	if (duk_get_top(ctx) != 1) return duk_generic_error(ctx, "milliseconds expected.");
	int sec = duk_get_int(ctx, 0);
	sleep(sec);
	return 0;
}

duk_ret_t athena_getFreeMemory(duk_context *ctx)
{
	if (duk_get_top(ctx) != 0) return duk_generic_error(ctx, "no arguments expected.");
	
	size_t result = GetFreeSize();

	duk_push_int(ctx, (uint32_t)(result));
	return 1;
}

duk_ret_t athena_exit(duk_context *ctx)
{
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "System.exitToBrowser");
	asm volatile(
            "li $3, 0x04;"
            "syscall;"
            "nop;"
        );
	return 0;
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

duk_ret_t athena_getmcinfo(duk_context *ctx){
	int argc = duk_get_top(ctx);
	int mcslot, type, freespace, format, result;
	mcslot = 0;
	if(argc == 1) mcslot = duk_get_int(ctx, 0);

	mcGetInfo(mcslot, 0, &type, &freespace, &format);
	mcSync(0, NULL, &result);

	duk_idx_t obj_idx = duk_push_object(ctx);

    duk_push_uint(ctx, type);
	duk_put_prop_string(ctx, obj_idx, "type");
    
    duk_push_uint(ctx, freespace);
	duk_put_prop_string(ctx, obj_idx, "freemem");

    duk_push_uint(ctx, format);
	duk_put_prop_string(ctx, obj_idx, "format");

	return 1;
}

duk_ret_t athena_openfile(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "wrong number of arguments");
	const char *file_tbo = duk_get_string(ctx, 0);
	int type = duk_get_int(ctx, 1);
	int fileHandle = open(file_tbo, type, 0777);
	if (fileHandle < 0) return duk_generic_error(ctx, "cannot open requested file.");
	duk_push_int(ctx,fileHandle);
	return 1;
}


duk_ret_t athena_readfile(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "wrong number of arguments");
	int file = duk_get_int(ctx, 0);
	uint32_t size = duk_get_int(ctx, 1);
	uint8_t *buffer = (uint8_t*)malloc(size + 1);
	int len = read(file,buffer, size);
	buffer[len] = 0;
	duk_push_lstring(ctx, (const char*)buffer, len);
	free(buffer);
	return 1;
}


duk_ret_t athena_writefile(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments");
	int fileHandle = duk_get_int(ctx, 0);
	const char *text = duk_get_string(ctx, 1);
	int size = duk_get_number(ctx, 2);
	write(fileHandle, text, size);
	return 0;
}

duk_ret_t athena_closefile(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	int fileHandle = duk_get_int(ctx, 0);
	close(fileHandle);
	return 0;
}

duk_ret_t athena_seekfile(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 3) return duk_generic_error(ctx, "wrong number of arguments");
	int fileHandle = duk_get_int(ctx, 0);
	int pos = duk_get_int(ctx, 1);
	uint32_t type = duk_get_int(ctx, 2);
	lseek(fileHandle, pos, type);	
	return 0;
}


duk_ret_t athena_sizefile(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	int fileHandle = duk_get_int(ctx, 0);
	uint32_t cur_off = lseek(fileHandle, 0, SEEK_CUR);
	uint32_t size = lseek(fileHandle, 0, SEEK_END);
	lseek(fileHandle, cur_off, SEEK_SET);
	duk_push_int(ctx, size);
	return 1;
}


duk_ret_t athena_checkexist(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1) return duk_generic_error(ctx, "wrong number of arguments");
	const char *file_tbo = duk_get_string(ctx, 0);
	int fileHandle = open(file_tbo, O_RDONLY, 0777);
	if (fileHandle < 0) duk_push_boolean(ctx, false);
	else{
		close(fileHandle);
		duk_push_boolean(ctx, true);
	}
	return 1;
}


duk_ret_t athena_loadELF(duk_context *ctx)
{
	size_t size;
	const char *elftoload = duk_get_lstring(ctx, 1, &size);
	if (!elftoload) return duk_generic_error(ctx, "Argument error: System.loadELF() takes a string as argument.");
	load_elf_NoIOPReset(elftoload);
	return 1;
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


duk_ret_t athena_checkValidDisc(duk_context *ctx)
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
	duk_push_int(ctx, result); //return the value itself to Lua stack
    return 1; //return value quantity on stack
}

duk_ret_t athena_checkDiscTray(duk_context *ctx)
{
	int result;
	if (sceCdStatus() == SCECdStatShellOpen){
		result = 1;
	} else {
		result = 0;
	}
	duk_push_int(ctx, result); //return the value itself to Lua stack
    return 1; //return value quantity on stack
}


duk_ret_t athena_getDiscType(duk_context *ctx)
{
    int discType;
    int iz;
    discType = sceCdGetDiskType();
    
    int DiscType_ix = 0;
        for (iz = 0; DiscTypes[iz].name[0]; iz++)
            if (DiscTypes[iz].type == discType)
                DiscType_ix = iz;
    printf("getDiscType: %d\n",DiscTypes[DiscType_ix].value);
    duk_push_int(ctx, DiscTypes[DiscType_ix].value); //return the value itself to Lua stack
    return 1; //return value quantity on stack
}

extern void *_gp;

#define BUFSIZE (64*1024)

static volatile off_t progress, max_progress;

struct pathMap {
	const char* in;
	const char* out;
};

static int copyThread(void* data)
{
	struct pathMap* paths = (struct pathMap*)data;

    char buffer[BUFSIZE];
    int in = open(paths->in, O_RDONLY, 0);
    int out = open(paths->out, O_WRONLY | O_CREAT | O_TRUNC, 644);

    // Get the input file size
	uint32_t size = lseek(in, 0, SEEK_END);
	lseek(in, 0, SEEK_SET);

    progress = 0;
    max_progress = size;

    ssize_t bytes_read;
    while((bytes_read = read(in, buffer, BUFSIZE)) > 0)
    {
        write(out, buffer, bytes_read);
        progress += bytes_read;
    }

    // copy is done, or an error occurred
    close(in);
    close(out);
	free(paths);
	ExitDeleteThread();
    return 0;
}


duk_ret_t athena_copyasync(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2) return duk_generic_error(ctx, "wrong number of arguments");

	struct pathMap* copypaths = (struct pathMap*)malloc(sizeof(struct pathMap));

	copypaths->in = duk_get_string(ctx, 0);
	copypaths->out = duk_get_string(ctx, 1);
	
	static u8 copyThreadStack[65*1024] __attribute__((aligned(16)));
	
	ee_thread_t thread_param;
	
	thread_param.gp_reg = &_gp;
    thread_param.func = (void*)copyThread;
    thread_param.stack = (void *)copyThreadStack;
    thread_param.stack_size = sizeof(copyThreadStack);
    thread_param.initial_priority = 0x12;
	int thread = CreateThread(&thread_param);
	
	StartThread(thread, (void*)copypaths);
	return 0;
}


duk_ret_t athena_getfileprogress(duk_context *ctx) {
	int argc = duk_get_top(ctx);
	if (argc != 0) return duk_generic_error(ctx, "wrong number of arguments");

	duk_idx_t obj_idx = duk_push_object(ctx);

	duk_push_int(ctx, (int)progress);
    duk_put_prop_string(ctx, obj_idx, "current");

	duk_push_int(ctx, (int)max_progress);
    duk_put_prop_string(ctx, obj_idx, "final");

	return 1;
}

DUK_EXTERNAL duk_ret_t dukopen_system(duk_context *ctx) {
	setModulePath();
	const duk_function_list_entry module_funcs[] = {
		{ "openFile",                   athena_openfile,					  2 },
		{ "readFile",                   athena_readfile,					  2 },
		{ "writeFile",                 	athena_writefile,					  3 },
		{ "closeFile",                 	athena_closefile,					  1 },  
		{ "seekFile",                   athena_seekfile,					  3 },  
		{ "sizeFile",                   athena_sizefile,					  1 },
		{ "doesFileExist",            	athena_checkexist,					  1 },
		{ "currentDirectory",           athena_curdir,				DUK_VARARGS },
		{ "listDirectory",           	athena_dir,					DUK_VARARGS },
		{ "createDirectory",           	athena_createDir,					  1 },
		{ "removeDirectory",           	athena_removeDir,					  1 },
		{ "moveFile",	               	athena_movefile,					  2 },
		{ "copyFile",	               	athena_copyfile,					  2 },
		{ "threadCopyFile",	          	athena_copyasync,					  2 },
		{ "getFileProgress",	    	athena_getfileprogress,				  0 },
		{ "removeFile",               	athena_removeFile,					  2 },
		{ "rename",                     athena_rename,						  2 },
		{ "sleep",                      athena_sleep,						  1 },
		{ "getFreeMemory",         		athena_getFreeMemory,				  0 },
		{ "exitToBrowser",              athena_exit,						  0 },
		{ "getMCInfo",                 	athena_getmcinfo,			DUK_VARARGS },
		{ "loadELF",                 	athena_loadELF,						  1 },
		{ "checkValidDisc",       		athena_checkValidDisc,				  0 },
		{ "getDiscType",             	athena_getDiscType,					  0 },
		{ "checkDiscTray",         		athena_checkDiscTray,				  0 },
		{ NULL, NULL, 0 }
	};

    duk_push_object(ctx);  /* module result */
    duk_put_function_list(ctx, -1, module_funcs);

    return 1;  /* return module value */
}

duk_ret_t athena_sifloadmodule(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 1 && argc != 3) return duk_generic_error(ctx, "wrong number of arguments");
	const char *path = duk_get_string(ctx, 0);

	int arg_len = 0;
	const char *args = NULL;

	if(argc == 3){
		arg_len = duk_get_int(ctx, 1);
		args = duk_get_string(ctx, 2);
	}
	

	int result = SifLoadModule(path, arg_len, args);
	duk_push_int(ctx, result);
	return 1;
}


duk_ret_t athena_sifloadmodulebuffer(duk_context *ctx){
	int argc = duk_get_top(ctx);
	if (argc != 2 && argc != 4) return duk_generic_error(ctx, "wrong number of arguments");
	const char* ptr = duk_get_string(ctx, 0);
	int size = duk_get_int(ctx, 1);

	int arg_len = 0;
	const char *args = NULL;

	if(argc == 4){
		arg_len = duk_get_int(ctx, 2);
		args = duk_get_string(ctx, 3);
	}

	int result = SifExecModuleBuffer((void*)ptr, size, arg_len, args, NULL);
	duk_push_int(ctx, result);
	return 1;
}

DUK_EXTERNAL duk_ret_t dukopen_sif(duk_context *ctx) {
	const duk_function_list_entry module_funcs[] = {
		{"loadModule",             		 athena_sifloadmodule, 			DUK_VARARGS },
		{"loadModuleBuffer",             athena_sifloadmodulebuffer, 	DUK_VARARGS },

		{NULL, NULL, 0}
	};

    duk_push_object(ctx);  /* module result */
    duk_put_function_list(ctx, -1, module_funcs);

    return 1;  /* return module value */
}


void athena_system_init(duk_context* ctx){
	push_athena_module(dukopen_system,   	 "System");
	push_athena_module(dukopen_sif,   	  		"Sif");

	duk_push_uint(ctx, O_RDONLY);
	duk_put_global_string(ctx, "FREAD");

	duk_push_uint(ctx, O_WRONLY);
	duk_put_global_string(ctx, "FWRITE");

	duk_push_uint(ctx, O_CREAT | O_WRONLY);
	duk_put_global_string(ctx, "FCREATE");

	duk_push_uint(ctx, O_RDWR);
	duk_put_global_string(ctx, "FRDWR");
	
	duk_push_uint(ctx, SEEK_SET);
	duk_put_global_string(ctx, "SET");

	duk_push_uint(ctx, SEEK_END);
	duk_put_global_string(ctx, "END");

	duk_push_uint(ctx, SEEK_CUR);
	duk_put_global_string(ctx, "CUR");

	duk_push_uint(ctx, 1);
	duk_put_global_string(ctx, "READ_ONLY");

	duk_push_uint(ctx, 2);
	duk_put_global_string(ctx, "READ_WRITE");
    

}

