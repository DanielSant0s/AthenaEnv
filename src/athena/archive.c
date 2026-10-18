#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <zip.h>
#include <zlib.h>
#include <dbgprintf.h>

#include <athena/archive.h>
#include <athena_core.h>

static int archive_parse_oct(const char *p, size_t n)
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
    return i;
}

static int archive_is_end_of_archive(const char *p)
{
    for (int n = 511; n >= 0; --n)
        if (p[n] != '\0')
            return 0;
    return 1;
}

static void archive_create_dir(char *pathname, int mode)
{
    char *p;
    int r;
    size_t n;

    if (!pathname || !pathname[0])
        return;

    n = strlen(pathname);
    if (pathname[n - 1] == '/')
        pathname[n - 1] = '\0';

    r = mkdir(pathname, mode);
    if (r != 0) {
        p = strrchr(pathname, '/');
        if (p != NULL) {
            *p = '\0';
            archive_create_dir(pathname, 0755);
            *p = '/';
            r = mkdir(pathname, mode);
        }
    }
    if (r != 0)
        dbgprintf("Could not create directory %s\n", pathname);
}

static FILE *archive_create_file(char *pathname, int mode)
{
    (void)mode;
    FILE *f = fopen(pathname, "wb+");
    if (f == NULL) {
        char *p = strrchr(pathname, '/');
        if (p != NULL) {
            *p = '\0';
            archive_create_dir(pathname, 0755);
            *p = '/';
            f = fopen(pathname, "wb+");
        }
    }
    return f;
}

static int archive_verify_checksum(const char *p)
{
    int n, u = 0;
    for (n = 0; n < 512; ++n) {
        if (n < 148 || n > 155)
            u += ((unsigned char *)p)[n];
        else
            u += 0x20;
    }
    return (u == archive_parse_oct(p + 148, 8));
}

static int archive_join_path(char *dst, size_t dst_size, const char *base, const char *name)
{
    size_t base_len;
    size_t name_len;
    int need_sep;

    if (!dst || dst_size == 0)
        return -1;
    if (!base)
        base = "";
    if (!name)
        name = "";

    base_len = strlen(base);
    name_len = strlen(name);
    need_sep = (base_len > 0 && name_len > 0 &&
                base[base_len - 1] != '/' && base[base_len - 1] != ':' &&
                name[0] != '/');

    if (base_len + name_len + (need_sep ? 1 : 0) >= dst_size)
        return -1;

    memcpy(dst, base, base_len);
    if (need_sep)
        dst[base_len++] = '/';
    memcpy(dst + base_len, name, name_len + 1);
    return 0;
}

static int archive_resolve_path(char *dst, size_t dst_size, const char *path)
{
    if (!path || !path[0])
        return -1;
    if (strchr(path, ':') || path[0] == '/') {
        if (strlen(path) >= dst_size)
            return -1;
        strcpy(dst, path);
        return 0;
    }
    return archive_join_path(dst, dst_size, boot_path, path);
}

static void archive_untar(FILE *a, const char *path)
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
            dbgprintf("Short read on %s: expected 512, got %d\n", path, (int)bytes_read);
            return;
        }
        if (archive_is_end_of_archive(buff)) {
            dbgprintf("End of %s\n", path);
            return;
        }
        if (!archive_verify_checksum(buff)) {
            dbgprintf("Checksum failure\n");
            return;
        }
        filesize = archive_parse_oct(buff + 124, 12);
        switch (buff[156]) {
        case '1':
        case '2':
        case '3':
        case '4':
            break;
        case '5':
            dbgprintf(" Extracting dir %s\n", buff);
            if (archive_join_path(out_buff, sizeof(out_buff), path, buff) == 0)
                archive_create_dir(out_buff, archive_parse_oct(buff + 100, 8));
            filesize = 0;
            break;
        case '6':
            break;
        default:
            dbgprintf(" Extracting file %s\n", buff);
            if (archive_join_path(out_buff, sizeof(out_buff), path, buff) == 0)
                f = archive_create_file(out_buff, archive_parse_oct(buff + 100, 8));
            break;
        }
        while (filesize > 0) {
            bytes_read = fread(buff, 1, 512, a);
            if (bytes_read < 512) {
                dbgprintf("Short read on %s: Expected 512, got %d\n", path, (int)bytes_read);
                return;
            }
            if (filesize < 512)
                bytes_read = filesize;
            if (f != NULL) {
                if (fwrite(buff, 1, bytes_read, f) != bytes_read) {
                    dbgprintf("Failed write\n");
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

AthenaArchive *athena_archive_open(const char *path)
{
    FILE *fp;
    AthenaArchive *archive;
    char buf[100];
    char path_buf[512];
    int err;
    uint32_t magic;

    if (!path)
        return NULL;

    archive = malloc(sizeof(*archive));
    if (!archive)
        return NULL;

    fp = fopen(path, "rb");
    if (!fp) {
        free(archive);
        return NULL;
    }

    fread(&magic, 1, 4, fp);
    fclose(fp);

    if (magic == 0x04034b50) {
        if (archive_resolve_path(path_buf, sizeof(path_buf), path) != 0) {
            free(archive);
            return NULL;
        }
        archive->handle = zip_open(path_buf, 0, &err);
        if (!archive->handle) {
            zip_error_to_str(buf, sizeof(buf), err, errno);
            dbgprintf("AthenaZip: can't open zip archive `%s': %s\n", path, buf);
            free(archive);
            return NULL;
        }
        archive->type = ATHENA_ARCHIVE_ZIP;
    } else if ((magic & 0x0000FFFF) == 0x8b1f) {
        archive->handle = gzopen(path, "rb");
        if (!archive->handle) {
            free(archive);
            return NULL;
        }
        archive->type = ATHENA_ARCHIVE_GZ;
    } else {
        free(archive);
        return NULL;
    }

    return archive;
}

AthenaArchiveEntryListing *athena_archive_list(AthenaArchive *archive)
{
    AthenaArchiveEntryListing *listing;
    struct zip_stat sb;

    if (!archive || archive->type != ATHENA_ARCHIVE_ZIP || !archive->handle)
        return NULL;

    listing = calloc(1, sizeof(*listing));
    if (!listing)
        return NULL;

    zip_t *zip = archive->handle;
    int num = zip_get_num_entries(zip, 0);
    for (int i = 0; i < num; i++) {
        if (zip_stat_index(zip, i, 0, &sb) != 0)
            continue;

        listing->entries = realloc(listing->entries,
            (listing->count + 1) * sizeof(AthenaArchiveEntry));
        if (!listing->entries)
            break;

        AthenaArchiveEntry *entry = &listing->entries[listing->count];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->name, sb.name, sizeof(entry->name) - 1);
        entry->size = sb.size;
        entry->mtime = sb.mtime;
        listing->count++;
    }

    return listing;
}

void athena_archive_entry_listing_free(AthenaArchiveEntryListing *listing)
{
    if (!listing)
        return;
    free(listing->entries);
    free(listing);
}

int athena_archive_extract_all(AthenaArchive *archive, void **out_data, size_t *out_size)
{
    if (!archive)
        return -1;

    if (archive->type == ATHENA_ARCHIVE_GZ) {
        gzFile gz_fp = archive->handle;
        unsigned char buf[8192];
        unsigned char *out = NULL;
        size_t total_bytes = 0;
        int bytes_read;

        while (1) {
            bytes_read = gzread(gz_fp, buf, sizeof(buf));
            if (bytes_read <= 0)
                break;
            out = realloc(out, total_bytes + bytes_read);
            if (!out)
                return -1;
            memcpy(out + total_bytes, buf, bytes_read);
            total_bytes += bytes_read;
            if (bytes_read < (int)sizeof(buf) && gzeof(gz_fp))
                break;
        }

        if (out_data)
            *out_data = out;
        else
            free(out);
        if (out_size)
            *out_size = total_bytes;
        return 0;
    }

    if (archive->type == ATHENA_ARCHIVE_ZIP) {
        zip_t *zip = archive->handle;
        struct zip_stat sb;
        unsigned char buf[8192];

        for (int i = 0; i < zip_get_num_entries(zip, 0); i++) {
            if (zip_stat_index(zip, i, 0, &sb) != 0)
                continue;

            size_t name_len = strlen(sb.name);
            if (name_len > 0 && sb.name[name_len - 1] == '/') {
                char outbuff[512];
                if (archive_join_path(outbuff, sizeof(outbuff), boot_path, sb.name) != 0)
                    continue;
                mkdir(outbuff, 0755);
            } else {
                struct zip_file *zf = zip_fopen_index(zip, i, 0);
                FILE *fp = fopen(sb.name, "wb+");
                long long sum = 0;
                int len;

                if (!zf || !fp) {
                    if (zf)
                        zip_fclose(zf);
                    if (fp)
                        fclose(fp);
                    continue;
                }

                while (sum != (long long)sb.size) {
                    len = zip_fread(zf, buf, 100);
                    if (len <= 0)
                        break;
                    fwrite(buf, 1, len, fp);
                    sum += len;
                }
                fclose(fp);
                zip_fclose(zf);
            }
        }
        return 0;
    }

    return -1;
}

int athena_archive_close(AthenaArchive *archive)
{
    if (!archive)
        return -1;

    if (archive->type == ATHENA_ARCHIVE_ZIP && archive->handle) {
        if (zip_close(archive->handle) == -1) {
            dbgprintf("AthenaZip: can't close zip archive\n");
            return -1;
        }
        archive->handle = NULL;
    } else if (archive->type == ATHENA_ARCHIVE_GZ && archive->handle) {
        gzclose(archive->handle);
        archive->handle = NULL;
    }

    return 0;
}

void athena_archive_destroy(AthenaArchive *archive)
{
    if (!archive)
        return;
    athena_archive_close(archive);
    free(archive);
}

int athena_archive_untar(const char *path)
{
    FILE *fp;
    gzFile gz_fp;
    char tar_path[512];
    char tmp_path[512];
    unsigned char f_buf[8192];
    uint32_t magic;
    int is_gz = 0;
    int n;
    size_t len;

    if (!path)
        return -1;

    fp = fopen(path, "rb");
    if (!fp)
        return -1;

    if (fread(&magic, 1, 4, fp) != 4) {
        fclose(fp);
        return -1;
    }
    fseek(fp, 0, SEEK_SET);

    if ((magic & 0x0000FFFF) == 0x8b1f) {
        fclose(fp);
        fp = NULL;

        gz_fp = gzopen(path, "rb");
        if (!gz_fp)
            return -1;

        if (snprintf(tmp_path, sizeof(tmp_path), "%s.untar", path) >= (int)sizeof(tmp_path)) {
            gzclose(gz_fp);
            return -1;
        }

        fp = fopen(tmp_path, "wb+");
        if (!fp) {
            gzclose(gz_fp);
            return -1;
        }

        while ((n = gzread(gz_fp, f_buf, sizeof(f_buf))) > 0) {
            if (fwrite(f_buf, 1, (size_t)n, fp) != (size_t)n) {
                fclose(fp);
                gzclose(gz_fp);
                remove(tmp_path);
                return -1;
            }
        }
        gzclose(gz_fp);
        fseek(fp, 0, SEEK_SET);
        is_gz = 1;
    }

    if (!getcwd(tar_path, sizeof(tar_path)))
        tar_path[0] = '\0';
    len = strlen(tar_path);
    if (len > 0 && tar_path[len - 1] != '/' && tar_path[len - 1] != ':' &&
        len + 1 < sizeof(tar_path)) {
        tar_path[len] = '/';
        tar_path[len + 1] = '\0';
    }

    archive_untar(fp, tar_path);
    fclose(fp);
    if (is_gz)
        remove(tmp_path);
    return 0;
}
