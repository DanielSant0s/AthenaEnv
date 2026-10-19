#include "ath_lua.h"
#include "ath_lua_graphics.h"
#include <athena/image.h>
#include <athena/video.h>

/*
 * Video (MPEG1/2 via libmpeg). Built in with STATIC_MPEG_VIDEO=1 or as
 * athena_mpeg_lua.erl, loaded on first use of the Video global.
 */

#define VIDEO_CLASS "Video"

typedef struct {
    AthenaVideo *video;
} LuaVideo;

static AthenaVideo *check_video(lua_State *L, int idx)
{
    LuaVideo *v = athena_lua_check_object(L, idx, VIDEO_CLASS);
    if (!v->video)
        luaL_error(L, "video was freed");
    return v->video;
}

static int l_video_new(lua_State *L)
{
    const char *path = luaL_checkstring(L, 1);
    AthenaVideo *video = athena_video_create(path);
    if (!video)
        return luaL_error(L, "failed to create MPEG player for '%s'", path);
    LuaVideo *v = athena_lua_new_object(L, VIDEO_CLASS, sizeof(LuaVideo));
    v->video = video;
    return 1;
}

static int l_video_free(lua_State *L)
{
    LuaVideo *v = athena_lua_check_object(L, 1, VIDEO_CLASS);
    if (v->video)
        athena_video_destroy(v->video);
    v->video = NULL;
    return 0;
}

static int l_video_play(lua_State *L) { athena_video_play(check_video(L, 1)); return 0; }
static int l_video_pause(lua_State *L) { athena_video_pause(check_video(L, 1)); return 0; }
static int l_video_stop(lua_State *L) { athena_video_stop(check_video(L, 1)); return 0; }
static int l_video_update(lua_State *L) { lua_pushboolean(L, athena_video_update(check_video(L, 1))); return 1; }

static int l_video_draw(lua_State *L)
{
    athena_video_draw(check_video(L, 1), athena_lua_optfloat(L, 2, 0), athena_lua_optfloat(L, 3, 0),
                      athena_lua_optfloat(L, 4, 0), athena_lua_optfloat(L, 5, 0));
    return 0;
}

static int get_video_width(lua_State *L) { lua_pushinteger(L, athena_video_get_width(check_video(L, 1))); return 1; }
static int get_video_height(lua_State *L) { lua_pushinteger(L, athena_video_get_height(check_video(L, 1))); return 1; }
static int get_video_fps(lua_State *L) { lua_pushnumber(L, athena_video_get_fps(check_video(L, 1))); return 1; }
static int get_video_ready(lua_State *L) { lua_pushboolean(L, athena_video_is_ready(check_video(L, 1))); return 1; }
static int get_video_ended(lua_State *L) { lua_pushboolean(L, athena_video_is_ended(check_video(L, 1))); return 1; }
static int get_video_playing(lua_State *L) { lua_pushboolean(L, athena_video_is_playing(check_video(L, 1))); return 1; }
static int get_video_loop(lua_State *L) { lua_pushboolean(L, athena_video_get_loop(check_video(L, 1))); return 1; }
static int set_video_loop(lua_State *L) { athena_video_set_loop(check_video(L, 1), athena_lua_tobool(L, 2)); return 0; }
static int get_video_current_frame(lua_State *L) { lua_pushinteger(L, athena_video_get_current_frame(check_video(L, 1))); return 1; }

/* video.frame: an Image viewing the decoded texture (usable on 3D meshes). */
static int get_video_frame(lua_State *L)
{
    AthenaVideo *video = check_video(L, 1);
    GSSURFACE *texture = athena_video_get_texture(video);
    if (!texture)
        return 0;
    AthenaImage *img = athena_image_wrap(texture, false);
    if (!img)
        return luaL_error(L, "out of memory");
    int w = athena_video_get_width(video), h = athena_video_get_height(video);
    img->width = img->endx = (float)w;
    img->height = img->endy = (float)h;
    athena_lua_push_image(L, img, true); /* wrap: destroying it leaves the surface alone */
    athena_lua_retain(L, -1, 1, "__video");
    return 1;
}

static const athena_lua_prop video_props[] = {
    { "width", get_video_width, NULL }, { "height", get_video_height, NULL },
    { "fps", get_video_fps, NULL }, { "ready", get_video_ready, NULL },
    { "ended", get_video_ended, NULL }, { "playing", get_video_playing, NULL },
    { "loop", get_video_loop, set_video_loop }, { "frame", get_video_frame, NULL },
    { "currentFrame", get_video_current_frame, NULL },
    { NULL, NULL, NULL }
};

static const luaL_Reg video_methods[] = {
    { "play", l_video_play }, { "pause", l_video_pause }, { "stop", l_video_stop },
    { "update", l_video_update }, { "draw", l_video_draw }, { "free", l_video_free },
    { NULL, NULL }
};

static const athena_lua_class video_class = {
    .name = VIDEO_CLASS, .methods = video_methods, .props = video_props, .gc = l_video_free,
};

static void open_video(lua_State *L)
{
    athena_lua_register_class(L, &video_class);
    athena_lua_new_module(L, NULL, NULL, l_video_new);
    lua_pushvalue(L, -1);
    athena_lua_set_global_module(L, VIDEO_CLASS);
}

#ifdef DYNAMIC_ATHENA_MPEG
char *erl_id = "athena_mpeg_lua";

int lua_init_module(lua_State *L)
{
    open_video(L);
    return 1;
}
#else
void athena_lua_open_video(lua_State *L)
{
    open_video(L);
    lua_pop(L, 1);
}
#endif
