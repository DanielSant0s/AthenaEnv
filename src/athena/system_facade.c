#include <unistd.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <dbgprintf.h>
#include <timer.h>

#include <athena/system_facade.h>
#include <athena_core.h>
#include <system.h>
#include <memory.h>
#include <taskman.h>

#include <usbhdfsd-common.h>
#include <hdd-ioctl.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <io_common.h>

extern void LoadELFFromFile(const char *path, int argc, char *argv[]);

static char *athena_system_resolve_path(const char *path, char *resolved, size_t resolved_size)
{
    if (!path || path[0] == '\0') {
        getcwd(resolved, resolved_size);
        return resolved;
    }

    if (strchr(path, ':')) {
        strncpy(resolved, path, resolved_size - 1);
        resolved[resolved_size - 1] = '\0';
        return resolved;
    }

    strncpy(resolved, boot_path, resolved_size - 1);
    resolved[resolved_size - 1] = '\0';
    strncat(resolved, path, resolved_size - strlen(resolved) - 1);
    return resolved;
}

AthenaDirListing *athena_system_list_dir(const char *path)
{
    AthenaDirListing *listing = calloc(1, sizeof(*listing));
    if (!listing)
        return NULL;

    char resolved[384];
    char tpath[384];
    const char *scan_path = athena_system_resolve_path(path, resolved, sizeof(resolved));

    if (strncmp(scan_path, "hdd", 3) == 0 && strlen(scan_path) <= 5) {
        iox_dirent_t dirent;
        int fd = fileXioDopen(strncpy(tpath, scan_path, sizeof(tpath)));
        if (fd >= 0) {
            while (fileXioDread(fd, &dirent) > 0) {
                if (dirent.stat.attr & APA_FLAG_SUB)
                    continue;
                if (strcmp(dirent.name, "__empty") == 0)
                    continue;

                listing->entries = realloc(listing->entries,
                    (listing->count + 1) * sizeof(AthenaDirEntry));
                if (!listing->entries)
                    break;

                AthenaDirEntry *entry = &listing->entries[listing->count];
                memset(entry, 0, sizeof(*entry));

                if (dirent.stat.mode != APA_TYPE_HDL)
                    strncpy(entry->name, dirent.name, sizeof(entry->name) - 1);
                else
                    snprintf(entry->name, sizeof(entry->name), "%s.iso", dirent.name);

                entry->size = 512U * dirent.stat.size * (dirent.stat.private_0 + 1);
                entry->is_dir = (dirent.stat.mode == APA_TYPE_PFS);
                listing->count++;
            }
            fileXioDclose(fd);
        }
        return listing;
    }

    DIR *d = opendir(scan_path);
    if (!d)
        return listing;

    struct dirent *dir;
    struct stat statbuf;
    while ((dir = readdir(d)) != NULL) {
        strcpy(tpath, scan_path);
        strcat(tpath, "/");
        strcat(tpath, dir->d_name);
        stat(tpath, &statbuf);

        listing->entries = realloc(listing->entries,
            (listing->count + 1) * sizeof(AthenaDirEntry));
        if (!listing->entries)
            break;

        AthenaDirEntry *entry = &listing->entries[listing->count];
        memset(entry, 0, sizeof(*entry));
        strncpy(entry->name, dir->d_name, sizeof(entry->name) - 1);
        entry->size = (uint32_t)statbuf.st_size;
        entry->is_dir = (dir->d_type == DT_DIR);
        listing->count++;
    }
    closedir(d);
    return listing;
}

void athena_system_dir_listing_free(AthenaDirListing *listing)
{
    if (!listing)
        return;
    free(listing->entries);
    free(listing);
}

int athena_system_stat(const char *path, AthenaFileStat *out)
{
    char resolved[384];
    struct stat statbuf;

    if (!path || !out)
        return -1;

    athena_system_resolve_path(path, resolved, sizeof(resolved));
    if (stat(resolved, &statbuf) != 0)
        return -1;

    out->size = (uint32_t)statbuf.st_size;
    out->is_dir = S_ISDIR(statbuf.st_mode);
    return 0;
}

static int athena_system_copy_internal(const char *src, const char *dst)
{
    char buf[BUFSIZ];
    size_t size;
    int source = open(src, O_RDONLY, 0);
    if (source < 0)
        return -1;

    int dest = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest < 0) {
        close(source);
        return -1;
    }

    while ((size = read(source, buf, BUFSIZ)) > 0)
        write(dest, buf, size);

    close(source);
    close(dest);
    return 0;
}

int athena_system_copy_file(const char *src, const char *dst)
{
    if (!src || !dst)
        return -1;
    return athena_system_copy_internal(src, dst);
}

int athena_system_move_file(const char *src, const char *dst)
{
    if (!src || !dst)
        return -1;
    if (athena_system_copy_internal(src, dst) != 0)
        return -1;
    return remove(src);
}

int athena_system_remove_file(const char *path)
{
    if (!path)
        return -1;
    return remove(path);
}

int athena_system_remove_dir(const char *path)
{
    if (!path)
        return -1;
    return rmdir(path);
}

void athena_system_sleep(int seconds)
{
    sleep(seconds);
}

void athena_system_poweroff(void)
{
    asm volatile(
        "li $3, 0x04;"
        "syscall;"
        "nop;"
    );
}

void athena_system_reboot(const char *elf_path, int argc, char **argv)
{
    if (elf_path)
        LoadELFFromFile(elf_path, argc, argv);
}

int athena_system_getcwd(char *buf, size_t size)
{
    if (!buf || size == 0)
        return -1;
    return getcwd(buf, size) != NULL ? 0 : -1;
}

int athena_system_chdir(const char *path)
{
    if (!path)
        return -1;
    return chdir(path);
}

int athena_system_load_elf(const char *path, int argc, char **argv, bool reset_iop)
{
    if (!path)
        return -1;

    if (reset_iop)
        LoadELFFromFile(path, argc, argv);

    return LoadELFFromFileNoReset(path, argc, argv);
}

AthenaThreadListing *athena_system_list_threads(void)
{
    Task *tasks = get_tasks();
    AthenaThreadListing *listing = calloc(1, sizeof(*listing));
    if (!listing || !tasks)
        return listing;

    for (int i = 0; i < 256; i++) {
        if (is_invalid_task(&tasks[i]))
            continue;

        listing->threads = realloc(listing->threads,
            (listing->count + 1) * sizeof(AthenaThreadInfo));
        if (!listing->threads)
            break;

        AthenaThreadInfo *info = &listing->threads[listing->count];
        info->id = tasks[i].id;
        info->title = tasks[i].title;
        info->stack_size = tasks[i].stack_size;
        info->status = tasks[i].status;
        listing->count++;
    }

    return listing;
}

void athena_system_thread_listing_free(AthenaThreadListing *listing)
{
    if (!listing)
        return;
    free(listing->threads);
    free(listing);
}

void athena_system_get_memory_stats(AthenaMemoryStats *stats)
{
    if (!stats)
        return;

    AthenaMemCompressStats cstats;
    athena_mem_compress_get_stats(&cstats);

    stats->core = (uint32_t)get_binary_size();
    stats->native_stack = (uint32_t)get_stack_size();
    stats->allocs = (uint32_t)get_allocs_size();
    stats->used = (uint32_t)get_used_memory();
    stats->compressed_bytes = (uint32_t)cstats.compressed_bytes;
    stats->uncompressed_bytes = (uint32_t)cstats.uncompressed_bytes;
    stats->saved_bytes = (uint32_t)cstats.saved_bytes;
    stats->compressed_textures = cstats.compressed_textures;
    stats->tracked_textures = cstats.tracked_textures;
    stats->compressed_meshes = cstats.compressed_meshes;
    stats->tracked_meshes = cstats.tracked_meshes;
    stats->compress_count = cstats.compress_count;
    stats->decompress_count = cstats.decompress_count;
    stats->oom_rescues = cstats.oom_rescues;
    stats->compression_enabled = cstats.enabled;
    stats->frame_budget_us = cstats.frame_budget_us;
}
