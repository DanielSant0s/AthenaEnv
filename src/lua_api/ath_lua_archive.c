#include <stdlib.h>

#include "ath_lua.h"
#include <athena/archive.h>

/*
 * Archive (ZIP/GZ/TAR). Built in with STATIC_ARCHIVE=1 or as
 * athena_archive_lua.erl, loaded on first use of the Archive global.
 * Archive.open() returns an object; the JS-style handle functions
 * (Archive.list(a), Archive.close(a)...) work on it too.
 */

#define ARCHIVE_CLASS "Archive"

typedef struct {
    AthenaArchive *archive;
} LuaArchive;

static AthenaArchive *check_archive(lua_State *L, int idx)
{
    LuaArchive *a = athena_lua_check_object(L, idx, ARCHIVE_CLASS);
    if (!a->archive)
        luaL_error(L, "archive is closed");
    return a->archive;
}

static int l_archive_open(lua_State *L)
{
    AthenaArchive *archive = athena_archive_open(luaL_checkstring(L, 1));
    if (!archive)
        return 0;
    LuaArchive *a = athena_lua_new_object(L, ARCHIVE_CLASS, sizeof(LuaArchive));
    a->archive = archive;
    return 1;
}

static int l_archive_close(lua_State *L)
{
    LuaArchive *a = athena_lua_check_object(L, 1, ARCHIVE_CLASS);
    if (a->archive)
        athena_archive_close(a->archive);
    a->archive = NULL;
    return 0;
}

static int l_archive_list(lua_State *L)
{
    AthenaArchiveEntryListing *listing = athena_archive_list(check_archive(L, 1));
    if (!listing)
        return 0;
    lua_createtable(L, listing->count, 0);
    for (int i = 0; i < listing->count; i++) {
        lua_createtable(L, 0, 3);
        athena_lua_set_str(L, "name", listing->entries[i].name);
        athena_lua_set_int(L, "size", listing->entries[i].size);
        athena_lua_set_int(L, "mtime", listing->entries[i].mtime);
        lua_rawseti(L, -2, i + 1);
    }
    athena_archive_entry_listing_free(listing);
    return 1;
}

/* GZ: returns the decompressed data as a Buffer; ZIP: extracts to disk. */
static int l_archive_extract_all(lua_State *L)
{
    AthenaArchive *archive = check_archive(L, 1);
    if (archive->type == ATHENA_ARCHIVE_GZ) {
        void *out = NULL;
        size_t size = 0;
        if (athena_archive_extract_all(archive, &out, &size) != 0)
            return 0;
        athena_lua_push_buffer(L, out, (uint32_t)size, true);
        return 1;
    }
    lua_pushboolean(L, athena_archive_extract_all(archive, NULL, NULL) == 0);
    return 1;
}

static int l_archive_untar(lua_State *L)
{
    lua_pushboolean(L, athena_archive_untar(luaL_checkstring(L, 1)) == 0);
    return 1;
}

static const luaL_Reg archive_methods[] = {
    { "list", l_archive_list }, { "extractAll", l_archive_extract_all }, { "close", l_archive_close },
    { NULL, NULL }
};

static const luaL_Reg archive_funcs[] = {
    { "open", l_archive_open }, { "list", l_archive_list }, { "extractAll", l_archive_extract_all },
    { "close", l_archive_close }, { "untar", l_archive_untar },
    { NULL, NULL }
};

static const athena_lua_class archive_class = {
    .name = ARCHIVE_CLASS, .methods = archive_methods, .gc = l_archive_close,
};

static void open_archive(lua_State *L)
{
    athena_lua_register_class(L, &archive_class);
    athena_lua_new_module(L, archive_funcs, NULL, NULL);
    lua_pushvalue(L, -1);
    athena_lua_set_global_module(L, "Archive");
}

#ifdef DYNAMIC_ATHENA_ARCHIVE
char *erl_id = "athena_archive_lua";

int lua_init_module(lua_State *L)
{
    open_archive(L);
    return 1;
}
#else
void athena_lua_open_archive(lua_State *L)
{
    open_archive(L);
    lua_pop(L, 1);
}
#endif
