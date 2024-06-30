#include "ath_env.h"
#include <errno.h>
#include <zip.h>
#include <zlib.h>

#define ZIP_FILE 0
#define GZ_FILE 1

typedef struct {
    void* fp;
    char type;
} Archive;

/* Parse an octal number, ignoring leading and trailing nonsense. */
static int
parseoct(const char *p, size_t n)
{
	int i = 0;

	while ((*p < '0' || *p > '7') && n > 0) {
		++p;
		--n;
	}
	while (*p >= '0' && *p <= '7' && n > 0) {
		i *= 8;
		i += *p - '0';
		++p;
		--n;
	}
	return (i);
}

/* Returns true if this is 512 zero bytes. */
static int
is_end_of_archive(const char *p)
{
	int n;
	for (n = 511; n >= 0; --n)
		if (p[n] != '\0')
			return (0);
	return (1);
}

/* Create a directory, including parent directories as necessary. */
static void
create_dir(char *pathname, int mode)
{
	char *p;
	int r;

	/* Strip trailing '/' */
	if (pathname[strlen(pathname) - 1] == '/')
		pathname[strlen(pathname) - 1] = '\0';

	/* Try creating the directory. */
	r = mkdir(pathname, mode);

	if (r != 0) {
		/* On failure, try creating parent directory. */
		p = strrchr(pathname, '/');
		if (p != NULL) {
			*p = '\0';
			create_dir(pathname, 0755);
			*p = '/';

			r = mkdir(pathname, mode);
		}
	}
	if (r != 0)
		dbgprintf( "Could not create directory %s\n", pathname);
}

/* Create a file, including parent directory as necessary. */
static FILE *
create_file(char *pathname, int mode)
{
	FILE *f;
	f = fopen(pathname, "wb+");
	if (f == NULL) {
		/* Try creating parent dir and then creating file. */
		char *p = strrchr(pathname, '/');
		if (p != NULL) {
			*p = '\0';
			create_dir(pathname, 0755);
			*p = '/';
			f = fopen(pathname, "wb+");
		}
	}
	return (f);
}

/* Verify the tar checksum. */
static int
verify_checksum(const char *p)
{
	int n, u = 0;
	for (n = 0; n < 512; ++n) {
		if (n < 148 || n > 155)
			/* Standard tar checksum adds unsigned bytes. */
			u += ((unsigned char *)p)[n];
		else
			u += 0x20;

	}
	return (u == parseoct(p + 148, 8));
}

/* Extract a tar archive. */
static void
untar(FILE *a, const char *path)
{
	char buff[512];
    char out_buff[1024];
	FILE *f = NULL;
	size_t bytes_read;
	int filesize;

	dbgprintf("Extracting from %s\n", path);
	for (;;) {
		bytes_read = fread(buff, 1, 512, a);
		if (bytes_read < 512) {
			dbgprintf(
			    "Short read on %s: expected 512, got %d\n",
			    path, (int)bytes_read);
			return;
		}
		if (is_end_of_archive(buff)) {
			dbgprintf("End of %s\n", path);
			return;
		}
		if (!verify_checksum(buff)) {
			dbgprintf( "Checksum failure\n");
			return;
		}
		filesize = parseoct(buff + 124, 12);
		switch (buff[156]) {
		case '1':
		case '2':
		case '3':
		case '4':
			break;
		case '5':
			dbgprintf(" Extracting dir %s\n", buff);
            strcpy(out_buff, path);
            strcat(out_buff, buff);
			create_dir(out_buff, parseoct(buff + 100, 8));
			filesize = 0;
			break;
		case '6':
			break;
		default:
			dbgprintf(" Extracting file %s\n", buff);
			f = create_file(buff, parseoct(buff + 100, 8));
			break;
		}
		while (filesize > 0) {
			bytes_read = fread(buff, 1, 512, a);
			if (bytes_read < 512) {
				dbgprintf(
				    "Short read on %s: Expected 512, got %d\n",
				    path, (int)bytes_read);
				return;
			}
			if (filesize < 512)
				bytes_read = filesize;
			if (f != NULL) {
				if (fwrite(buff, 1, bytes_read, f)
				    != bytes_read)
				{
					dbgprintf( "Failed write\n");
					fclose(f);
					f = NULL;
				}
			}
			filesize -= bytes_read;
		}
		if (f != NULL) {
			fclose(f);
			f = NULL;
		}
	}
}

static JSValue athena_archiveopen(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    FILE* fp;
    Archive* za = malloc(sizeof(Archive));
    char buf[100];
    char path_buf[512];
    int err;

	const char *path = JS_ToCString(ctx, argv[0]);
    fp = fopen(path, "rb");

    uint32_t magic;
	fread(&magic, 1, 4, fp);	
	fclose(fp);

	if(magic == 0x04034b50 /* ZIP */) {
        strcpy(path_buf, boot_path);
		strcat(path_buf, "/");
        strcat(path_buf, path);
	    if ((za->fp = zip_open(path_buf, 0, &err)) == NULL) {
            zip_error_to_str(buf, sizeof(buf), err, errno);
            dbgprintf("AthenaZip: can't open zip archive `%s': %s\n",
                path, buf);
            return JS_UNDEFINED;
        }
        za->type = ZIP_FILE;
    } else if((magic & 0x0000FFFF) == 0x8b1f /* GZ */) {
        za->fp = gzopen(path, "rb");
        za->type = GZ_FILE;
    }

	JS_FreeCString(ctx, path);

	return JS_NewUint32(ctx, za);
}

static JSValue athena_archiveget(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	int len;
    Archive* za;
    struct zip_stat sb;
    JSValue arr, obj;
	JS_ToUint32(ctx, &za, argv[0]);

    arr = JS_NewArray(ctx);

	for (int i = 0; i < zip_get_num_entries(za->fp, 0); i++) {
        if (zip_stat_index(za->fp, i, 0, &sb) == 0) {
            obj = JS_NewObject(ctx);

			JS_DefinePropertyValueStr(ctx, obj, "name", JS_NewString(ctx, sb.name), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "size", JS_NewUint32(ctx, sb.size), JS_PROP_C_W_E);
			JS_DefinePropertyValueStr(ctx, obj, "mtime", JS_NewUint32(ctx, sb.mtime), JS_PROP_C_W_E);

			JS_DefinePropertyValueUint32(ctx, arr, i, obj, JS_PROP_C_W_E);
        } else {
            dbgprintf("File[%s] Line[%d]\n", __FILE__, __LINE__);
        }
    }
    
	return arr;
}

static JSValue athena_archiveclose(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
	Archive *za;
	JS_ToUint32(ctx, &za, argv[0]);

	if (zip_close(za->fp) == -1) {
        dbgprintf("AthenaZip: can't close zip archive\n");
    }

	return JS_UNDEFINED;
}

static JSValue athena_untar(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv){
    FILE* fp;
    gzFile gzFp;
    char buf[256];
	char tar_path[512];
    unsigned char f_buf[8192];
    unsigned char* out = NULL;
    size_t total_bytes = 0, bytes_read;
	const char *path = JS_ToCString(ctx, argv[0]);
    fp = fopen(path, "rb");

    uint32_t magic;
	fread(&magic, 1, 4, fp);
    fseek(fp, 0, SEEK_SET);

    if((magic & 0x0000FFFF) == 0x8b1f /* GZ */) {
        gzFp = gzopen(path, "rb");
        while(true) {
            bytes_read = gzread(gzFp, f_buf, 8192);
            total_bytes += bytes_read;
            out = realloc(out, total_bytes);
            memcpy((out+total_bytes-bytes_read), f_buf, bytes_read);
            if (bytes_read < 8192 && gzeof(gzFp)) {
                fclose(fp);
                dbgprintf("Total size : %d\n", total_bytes);

                size_t tar_name = strrchr(path, '.') - path;
                memcpy(buf, path, tar_name);
                buf[tar_name] = '\0';

                fp = fopen(buf, "wb+");
                fwrite(out, 1, total_bytes, fp);
                fclose(fp);

                fp = fopen(buf, "rb");

                break;
            }
        }
        gzclose(gzFp);
    }

	getcwd(tar_path, 512);
	strcat(tar_path, "/");
	untar(fp, tar_path);
	fclose(fp);

	JS_FreeCString(ctx, path);

	return JS_UNDEFINED;
}

static JSValue athena_extractall(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
    Archive* arc;
    FILE* fp;
    int len = 0;
    long long sum;
    struct zip_file *zf;
    char outbuff[512];
    struct zip_stat sb;
    int bytes_read;
    size_t total_bytes = 0;
    unsigned char buf[8192];
    unsigned char* out = NULL;
    JS_ToUint32(ctx, &arc, argv[0]);

    if (arc->type == GZ_FILE) {
        while(true) {
            bytes_read = gzread(arc->fp, buf, 8192);
            total_bytes += bytes_read;
            out = realloc(out, total_bytes);
            memcpy((out+total_bytes-bytes_read), buf, bytes_read);
            if (bytes_read < 8192) {
                if (gzeof(arc->fp)) {
                    return JS_NewArrayBuffer(ctx, out, total_bytes, NULL, NULL, 1);
                }
            }
        }
    } else if (arc->type == ZIP_FILE) {
        for (int i = 0; i < zip_get_num_entries(arc->fp, 0); i++) {
            if (zip_stat_index(arc->fp, i, 0, &sb) == 0) {
                if (sb.name[len - 1] == '/') {
                    strcpy(outbuff, boot_path);
					strcat(outbuff, "/");
                    strcat(outbuff, sb.name);
                    mkdir(outbuff, 0755);
                } else {
                    zf = zip_fopen_index(arc->fp, i, 0);
    
                    fp = fopen(sb.name, "wb+");
    
                    sum = 0;
                    while (sum != sb.size) {
                        len = zip_fread(zf, buf, 100);
                        fwrite(buf, 1, len, fp);
                        sum += len;
                    }
                    fclose(fp);
                    zip_fclose(zf);
                }
            } else {
                dbgprintf("File[%s] Line[%d]/n", __FILE__, __LINE__);
            }
        }   
    }
    return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
	JS_CFUNC_DEF("open", 1, athena_archiveopen),
	JS_CFUNC_DEF("list", 1, athena_archiveget),
    JS_CFUNC_DEF("extractAll", 1, athena_extractall),
	JS_CFUNC_DEF("close", 1, athena_archiveclose),
    JS_CFUNC_DEF("untar", 1, athena_untar),
};

static int module_init(JSContext *ctx, JSModuleDef *m)
{
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_archive_init(JSContext* ctx){
    return athena_push_module(ctx, module_init, module_funcs, countof(module_funcs), "Archive");
}