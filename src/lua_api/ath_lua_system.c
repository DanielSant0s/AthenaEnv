#include <unistd.h>
#include <string.h>
#include <kernel.h>
#include <malloc.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <libcdvd.h>
#include <timer.h>

#include "ath_lua.h"
#include <system.h>
#include <memory.h>
#include <def_mods.h>
#include <taskman.h>
#include <lz4_ps2.h>

#include <usbhdfsd-common.h>
#define NEWLIB_PORT_AWARE
#include <fileXio_rpc.h>
#include <fileio.h>
#include <io_common.h>

#include <n32_call.h>

#include <athena/system_facade.h>
#include <athena/sampler.h>
#include <erl.h>

extern int athena_lua_request_reload(lua_State *L, const char *script);
extern size_t athena_lua_mem_used(void);

static int l_listdir(lua_State *L)
{
    AthenaDirListing *listing = athena_system_list_dir(luaL_optstring(L, 1, NULL));

    lua_newtable(L);
    if (!listing)
        return 1;

    for (int i = 0; i < listing->count; i++) {
        lua_createtable(L, 0, 3);
        athena_lua_set_str(L, "name", listing->entries[i].name);
        athena_lua_set_int(L, "size", listing->entries[i].size);
        athena_lua_set_bool(L, "dir", listing->entries[i].is_dir);
        lua_rawseti(L, -2, i + 1);
    }

    athena_system_dir_listing_free(listing);
    return 1;
}

static int l_remove_dir(lua_State *L)
{
    athena_system_remove_dir(luaL_checkstring(L, 1));
    return 0;
}

static int l_move_file(lua_State *L)
{
    athena_system_move_file(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    return 0;
}

static int l_copy_file(lua_State *L)
{
    athena_system_copy_file(luaL_checkstring(L, 1), luaL_checkstring(L, 2));
    return 0;
}

static int l_sleep(lua_State *L)
{
    athena_system_sleep((int)luaL_checkinteger(L, 1));
    return 0;
}

static int l_delay(lua_State *L)
{
    nopdelay();
    return 0;
}

static int l_exit(lua_State *L)
{
    athena_system_poweroff();
    return 0;
}

static int l_reload(lua_State *L)
{
    return athena_lua_request_reload(L, luaL_optstring(L, 1, athena_get_default_script()));
}

static int l_getmcinfo(lua_State *L)
{
    int type, freespace, format, result;
    int slot = (int)luaL_optinteger(L, 1, 0);

    mcGetInfo(slot, 0, &type, &freespace, &format);
    mcSync(0, NULL, &result);

    lua_createtable(L, 0, 3);
    athena_lua_set_int(L, "type", type);
    athena_lua_set_int(L, "freemem", freespace);
    athena_lua_set_int(L, "format", format);
    return 1;
}

static int l_loadelf(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    int n = 0;
    char **args = NULL;

    if (lua_istable(L, 2)) {
        n = (int)luaL_len(L, 2);
        args = malloc(n * sizeof(char *));
        for (int i = 0; i < n; i++) {
            lua_geti(L, 2, i + 1);
            args[i] = (char *)luaL_checkstring(L, -1); /* kept alive by the table */
            lua_pop(L, 1);
        }
    }

    athena_system_load_elf(path, n, args, athena_lua_tobool(L, 3));
    return 0;
}

static int l_check_valid_disc(lua_State *L)
{
    int result = 0;
    switch (sceCdGetDiskType()) {
        case SCECdPSCD: case SCECdPSCDDA: case SCECdPS2CD: case SCECdPS2CDDA:
        case SCECdPS2DVD: case SCECdESRDVD_0: case SCECdESRDVD_1: case SCECdCDDA:
        case SCECdDVDV: case SCECdDETCTCD: case SCECdDETCTDVDS: case SCECdDETCTDVDD:
            result = 1;
            break;
    }
    lua_pushinteger(L, result);
    return 1;
}

static int l_check_disc_tray(lua_State *L)
{
    lua_pushinteger(L, sceCdStatus() == SCECdStatShellOpen);
    return 1;
}

static const struct { int type; int value; } disc_types[] = {
    { SCECdGDTFUNCFAIL, -1 }, { SCECdNODISC, 1 }, { SCECdDETCT, 2 }, { SCECdDETCTCD, 3 },
    { SCECdDETCTDVDS, 4 }, { SCECdDETCTDVDD, 5 }, { SCECdUNKNOWN, 6 }, { SCECdPSCD, 7 },
    { SCECdPSCDDA, 8 }, { SCECdPS2CD, 9 }, { SCECdPS2CDDA, 10 }, { SCECdPS2DVD, 11 },
    { SCECdESRDVD_0, 12 }, { SCECdESRDVD_1, 13 }, { SCECdCDDA, 14 }, { SCECdDVDV, 15 },
    { SCECdIllegalMedia, 16 },
};

static int l_get_disc_type(lua_State *L)
{
    int type = sceCdGetDiskType();
    int value = -1;
    for (unsigned i = 0; i < countof(disc_types); i++)
        if (disc_types[i].type == type)
            value = disc_types[i].value;
    lua_pushinteger(L, value);
    return 1;
}

static volatile off_t progress, max_progress;

struct path_map {
    char *in;
    char *out;
};

static int copy_thread(void *data)
{
    struct path_map *paths = (struct path_map *)data;
    char buffer[BUFSIZ];
    int in = open(paths->in, O_RDONLY, 0);
    int out = open(paths->out, O_WRONLY | O_CREAT | O_TRUNC, 644);

    uint32_t size = lseek(in, 0, SEEK_END);
    lseek(in, 0, SEEK_SET);

    progress = 0;
    max_progress = size;

    ssize_t bytes_read;
    while ((bytes_read = read(in, buffer, BUFSIZ)) > 0) {
        write(out, buffer, bytes_read);
        progress += bytes_read;
    }

    close(in);
    close(out);
    free(paths->in);
    free(paths->out);
    free(paths);
    exit_kill_task();
    return 0;
}

static int l_copy_async(lua_State *L)
{
    struct path_map *paths = malloc(sizeof(struct path_map));
    paths->in = strdup(luaL_checkstring(L, 1));
    paths->out = strdup(luaL_checkstring(L, 2));
    int task = create_task("FileSystem: Copy", (void *)copy_thread, 8192 + 1024, 100);
    init_task(task, (void *)paths);
    return 0;
}

static int l_get_file_progress(lua_State *L)
{
    lua_createtable(L, 0, 2);
    athena_lua_set_int(L, "current", (int)progress);
    athena_lua_set_int(L, "final", (int)max_progress);
    return 1;
}

static int l_mount(lua_State *L)
{
    lua_pushinteger(L, fileXioMount(luaL_checkstring(L, 1), luaL_checkstring(L, 2), (int)luaL_optinteger(L, 3, 0)));
    return 1;
}

static int l_umount(lua_State *L)
{
    lua_pushinteger(L, fileXioUmount(luaL_checkstring(L, 1)));
    return 1;
}

static int l_devices(lua_State *L)
{
    struct fileXioDevice devs[FILEXIO_MAX_DEVICES];
    int count = fileXioGetDeviceList(devs, FILEXIO_MAX_DEVICES);
    if (count <= 0)
        return 0;

    lua_createtable(L, count, 0);
    for (int i = 0; i < count; i++) {
        lua_createtable(L, 0, 2);
        athena_lua_set_str(L, "name", devs[i].name);
        athena_lua_set_str(L, "desc", devs[i].desc);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

static int l_get_bdm_info(lua_State *L)
{
    const char *mass = luaL_checkstring(L, 1);
    int fd = fileXioDopen(mass);
    if (fd < 0)
        return 0;

    char dev_name[10];
    int ok = fileXioIoctl2(fd, USBMASS_IOCTL_GET_DRIVERNAME, NULL, 0, dev_name, sizeof(dev_name) - 1) >= 0;
    fileXioDclose(fd);
    if (!ok)
        return 0;

    dev_name[sizeof(dev_name) - 1] = '\0';
    lua_createtable(L, 0, 2);
    athena_lua_set_str(L, "name", dev_name);
    athena_lua_set_int(L, "index", mass[4] - '0');
    return 1;
}

static int l_dark_mode(lua_State *L)
{
    dark_mode = athena_lua_tobool(L, 1);
    return 0;
}

#define GS_REG_CSR (volatile u64 *)0x12001000

static int l_get_cpu_info(lua_State *L)
{
    unsigned short revision = GetCop0(15);
    unsigned int value;

    lua_newtable(L);
    athena_lua_set_int(L, "implementation", (u8)(revision >> 8));
    athena_lua_set_int(L, "revision", (u8)(revision & 0xFF));

    asm("cfc1 %0, $0\n" : "=r"(revision) :);
    athena_lua_set_int(L, "FPUimplementation", (u8)(revision >> 8));
    athena_lua_set_int(L, "FPUrevision", (u8)(revision & 0xFF));

    value = GetCop0(16);
    athena_lua_set_int(L, "ICacheSize", (u8)(value >> 9 & 3));
    athena_lua_set_int(L, "DCacheSize", (u8)(value >> 6 & 3));
    athena_lua_set_int(L, "RAMSize", GetMemorySize());
    athena_lua_set_int(L, "BUSClock", kBUSCLK);
    athena_lua_set_int(L, "CPUClock", kBUSCLK * 2);
    athena_lua_set_int(L, "MachineType", MachineType());
    return 1;
}

static int l_get_gpu_info(lua_State *L)
{
    unsigned short revision = (*GS_REG_CSR) >> 16;
    lua_createtable(L, 0, 2);
    athena_lua_set_int(L, "revision", (u8)(revision & 0xFF));
    athena_lua_set_int(L, "id", (u8)(revision >> 8));
    return 1;
}

static int l_get_memory_stats(lua_State *L)
{
    AthenaMemoryStats stats;
    athena_system_get_memory_stats(&stats);

    lua_newtable(L);
    athena_lua_set_int(L, "core", stats.core);
    athena_lua_set_int(L, "nativeStack", stats.native_stack);
    athena_lua_set_int(L, "allocs", stats.allocs);
    athena_lua_set_int(L, "used", stats.used);
    athena_lua_set_int(L, "luaHeap", athena_lua_mem_used());
    athena_lua_set_int(L, "compressedBytes", stats.compressed_bytes);
    athena_lua_set_int(L, "uncompressedBytes", stats.uncompressed_bytes);
    athena_lua_set_int(L, "savedBytes", stats.saved_bytes);
    athena_lua_set_int(L, "compressedTextures", stats.compressed_textures);
    athena_lua_set_int(L, "trackedTextures", stats.tracked_textures);
    athena_lua_set_int(L, "compressedMeshes", stats.compressed_meshes);
    athena_lua_set_int(L, "trackedMeshes", stats.tracked_meshes);
    athena_lua_set_int(L, "compressCount", stats.compress_count);
    athena_lua_set_int(L, "decompressCount", stats.decompress_count);
    athena_lua_set_int(L, "oomRescues", stats.oom_rescues);
    athena_lua_set_bool(L, "compressionEnabled", stats.compression_enabled);
    athena_lua_set_int(L, "frameBudgetUs", stats.frame_budget_us);
    return 1;
}

static void read_compress_opts(lua_State *L, int idx, AthenaMemCompressConfig *cfg)
{
    athena_lua_field_u32(L, idx, "frameBudgetUs", &cfg->frame_budget_us);
    athena_lua_field_u32(L, idx, "textureVramFrames", &cfg->texture_vram_frames);
    athena_lua_field_u32(L, idx, "textureIdleFrames", &cfg->texture_idle_frames);
    athena_lua_field_u32(L, idx, "meshIdleFrames", &cfg->mesh_idle_frames);
}

static int l_set_mem_compression(lua_State *L)
{
    AthenaMemCompressConfig cfg;
    athena_mem_compress_get_config(&cfg);

    if (lua_istable(L, 1)) {
        athena_lua_field_bool(L, 1, "enabled", &cfg.enabled);
        read_compress_opts(L, 1, &cfg);
    } else if (!lua_isnone(L, 1)) {
        cfg.enabled = athena_lua_tobool(L, 1);
    }

    if (lua_istable(L, 2))
        read_compress_opts(L, 2, &cfg);

    athena_mem_compress_set_config(&cfg);

    lua_createtable(L, 0, 5);
    athena_lua_set_bool(L, "enabled", cfg.enabled);
    athena_lua_set_int(L, "frameBudgetUs", cfg.frame_budget_us);
    athena_lua_set_int(L, "textureVramFrames", cfg.texture_vram_frames);
    athena_lua_set_int(L, "textureIdleFrames", cfg.texture_idle_frames);
    athena_lua_set_int(L, "meshIdleFrames", cfg.mesh_idle_frames);
    return 1;
}

static int l_compact_memory(lua_State *L)
{
    lua_pushinteger(L, (lua_Integer)athena_mem_compact_now((size_t)athena_lua_optu32(L, 1, 0)));
    return 1;
}

static int l_lz4_compress(lua_State *L)
{
    uint32_t src_len;
    const uint8_t *src = athena_lua_to_bytes(L, 1, &src_len);
    if (!src || !src_len)
        return luaL_argerror(L, 1, "expected non-empty Buffer or string");

    size_t bound = athena_lz4_compress_bound(src_len);
    if (!bound)
        return luaL_error(L, "buffer too large for LZ4");

    uint8_t *dst = malloc(bound);
    if (!dst)
        return luaL_error(L, "out of memory");

    int csize = athena_lz4_compress(src, dst, (int)src_len, (int)bound);
    if (csize <= 0) {
        free(dst);
        return 0;
    }

    uint8_t *shrunk = realloc(dst, csize);
    athena_lua_push_buffer(L, shrunk ? shrunk : dst, csize, true);
    return 1;
}

static int l_lz4_decompress(lua_State *L)
{
    uint32_t src_len;
    const uint8_t *src = athena_lua_to_bytes(L, 1, &src_len);
    uint32_t size = athena_lua_checku32(L, 2);
    if (!src || !src_len)
        return luaL_argerror(L, 1, "expected non-empty Buffer or string");
    if (!size)
        return luaL_argerror(L, 2, "invalid uncompressed size");

    uint8_t *dst = malloc(size);
    if (!dst)
        return luaL_error(L, "out of memory");

    int dsize = athena_lz4_decompress(src, dst, (int)src_len, (int)size);
    if (dsize < 0) {
        free(dst);
        return 0;
    }

    athena_lua_push_buffer(L, dst, dsize, true);
    return 1;
}

static int l_profile_start(lua_State *L)
{
    lua_pushboolean(L, athena_sampler_start((int)luaL_optinteger(L, 1, 1000)) == 0);
    return 1;
}

static int l_profile_stop(lua_State *L)
{
    lua_pushinteger(L, athena_sampler_stop(luaL_optstring(L, 1, NULL)));
    return 1;
}

static int l_get_temperature(lua_State *L)
{
    unsigned char in_buffer[1] = { 0xEF }, out_buffer[16] = { 0 };
    int stat = 0;

    if (sceCdApplySCmd(0x03, in_buffer, sizeof(in_buffer), out_buffer) != 0)
        stat = out_buffer[0];

    if (stat)
        return 0;

    unsigned short temp = out_buffer[1] * 256 + out_buffer[2];
    lua_pushnumber(L, (float)((temp - (temp % 128)) / 128.0f) + (float)(temp % 128) / 10.0f);
    return 1;
}

static int l_load_reloc(lua_State *L)
{
    struct erl_record_t *erl = _init_load_erl_from_file((char *)luaL_checkstring(L, 1), 0);
    if (!erl)
        return 0;
    athena_lua_pushu32(L, (uint32_t)erl);
    return 1;
}

static int l_unload_reloc(lua_State *L)
{
    struct erl_record_t *erl = (struct erl_record_t *)athena_lua_checku32(L, 1);
    if (!erl)
        return 0;
    lua_pushinteger(L, unload_erl(erl));
    return 1;
}

static int l_find_reloc_object(lua_State *L)
{
    struct symbol_t *symbol = erl_find_symbol(luaL_checkstring(L, 1));
    if (!symbol)
        return 0;
    athena_lua_pushu32(L, (uint32_t)symbol->address);
    return 1;
}

static int l_find_reloc_local_object(lua_State *L)
{
    struct erl_record_t *erl = (struct erl_record_t *)athena_lua_checku32(L, 1);
    if (!erl)
        return 0;
    struct symbol_t *symbol = erl_find_local_symbol(luaL_checkstring(L, 2), erl);
    if (!symbol)
        return 0;
    athena_lua_pushu32(L, (uint32_t)symbol->address);
    return 1;
}

/* System.nativeCall(fnptr, { {type=T_INT, value=1}, ... }, returnType) */
static int l_native_call(lua_State *L)
{
    func_arg arg[32] = { 0 };
    void *func = (void *)athena_lua_checku32(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);
    uint32_t return_type = athena_lua_optu32(L, 3, NONTYPE_VOID);
    int arg_count = (int)luaL_len(L, 2);
    if (arg_count > 20)
        return luaL_error(L, "nativeCall supports at most 20 arguments");

    for (int i = 0; i < arg_count; i++) {
        lua_geti(L, 2, i + 1);
        int type = 0;
        athena_lua_field_int(L, -1, "type", &type);
        lua_getfield(L, -1, "value");

        switch (type) {
            case TYPE_LONG:
            case TYPE_ULONG:
                arg[i].ulong_arg = (int64_t)luaL_checknumber(L, -1);
                break;
            case TYPE_INT: case TYPE_UINT: case TYPE_SHORT: case TYPE_USHORT:
            case TYPE_CHAR: case TYPE_UCHAR: case TYPE_PTR:
                arg[i].uint_arg = athena_lua_checku32(L, -1);
                break;
            case TYPE_BOOL:
                arg[i].bool_arg = athena_lua_tobool(L, -1);
                break;
            case TYPE_FLOAT:
                process_float_arg((float)luaL_checknumber(L, -1), i);
                break;
            case TYPE_STRING:
                arg[i].string_arg = luaL_checkstring(L, -1); /* kept alive by the table */
                break;
            case TYPE_BUFFER: {
                uint32_t size;
                arg[i].ptr_arg = (void *)athena_lua_to_bytes(L, -1, &size);
                break;
            }
        }
        lua_pop(L, 2);
    }

#define CALL_ARGS arg[0], arg[1], arg[2], arg[3], arg[4], arg[5], arg[6], arg[7], arg[8], arg[9], \
                  arg[10], arg[11], arg[12], arg[13], arg[14], arg[15], arg[16], arg[17], arg[18], arg[19]

    switch (return_type) {
        case NONTYPE_VOID:
            call_function(void, func)(CALL_ARGS);
            return 0;
        case TYPE_INT: case TYPE_UINT: case TYPE_SHORT: case TYPE_USHORT:
        case TYPE_CHAR: case TYPE_UCHAR: case TYPE_PTR:
            lua_pushinteger(L, call_function(int, func)(CALL_ARGS));
            return 1;
        case TYPE_BOOL:
            lua_pushboolean(L, call_function(bool, func)(CALL_ARGS));
            return 1;
        case TYPE_FLOAT:
            lua_pushnumber(L, call_function(float, func)(CALL_ARGS));
            return 1;
        case TYPE_STRING:
            lua_pushstring(L, call_function(string, func)(CALL_ARGS));
            return 1;
    }
#undef CALL_ARGS

    return 0;
}

static const luaL_Reg system_funcs[] = {
    { "getBDMInfo", l_get_bdm_info },
    { "mount", l_mount },
    { "umount", l_umount },
    { "devices", l_devices },
    { "listDir", l_listdir },
    { "removeDirectory", l_remove_dir },
    { "moveFile", l_move_file },
    { "copyFile", l_copy_file },
    { "threadCopyFile", l_copy_async },
    { "getFileProgress", l_get_file_progress },
    { "rename", l_move_file },
    { "sleep", l_sleep },
    { "delay", l_delay },
    { "exitToBrowser", l_exit },
    { "reload", l_reload },
    { "getMCInfo", l_getmcinfo },
    { "loadELF", l_loadelf },
    { "checkValidDisc", l_check_valid_disc },
    { "getDiscType", l_get_disc_type },
    { "checkDiscTray", l_check_disc_tray },
    { "setDarkMode", l_dark_mode },
    { "getCPUInfo", l_get_cpu_info },
    { "getGPUInfo", l_get_gpu_info },
    { "getMemoryStats", l_get_memory_stats },
    { "setMemoryCompression", l_set_mem_compression },
    { "compactMemory", l_compact_memory },
    { "lz4Compress", l_lz4_compress },
    { "lz4Decompress", l_lz4_decompress },
    { "getTemperature", l_get_temperature },
    { "profileStart", l_profile_start },
    { "profileStop", l_profile_stop },
    { "nativeCall", l_native_call },
    { "loadReloc", l_load_reloc },
    { "unloadReloc", l_unload_reloc },
    { "findRelocObject", l_find_reloc_object },
    { "findRelocLocalObject", l_find_reloc_local_object },
    { NULL, NULL }
};

static const athena_lua_const system_consts[] = {
    { "T_LONG", TYPE_LONG }, { "T_ULONG", TYPE_ULONG }, { "T_INT", TYPE_INT },
    { "T_UINT", TYPE_UINT }, { "T_SHORT", TYPE_SHORT }, { "T_USHORT", TYPE_USHORT },
    { "T_CHAR", TYPE_CHAR }, { "T_UCHAR", TYPE_UCHAR }, { "T_BOOL", TYPE_BOOL },
    { "T_FLOAT", TYPE_FLOAT }, { "T_PTR", TYPE_PTR }, { "T_STRING", TYPE_STRING },
    { "T_BUFFER", TYPE_BUFFER },
    { "READ_ONLY", 1 }, { "SELECT", 2 },
    { NULL, 0 }
};

void athena_lua_open_system(lua_State *L)
{
    athena_lua_new_module(L, system_funcs, system_consts, NULL);
    athena_lua_set_str(L, "boot_path", boot_path);
    athena_lua_set_str(L, "engine", "lua");
    athena_lua_set_global_module(L, "System");
}
