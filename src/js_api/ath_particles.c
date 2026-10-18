#include <ath_env.h>
#include <athena/particles.h>
#include <math.h>
#include <string.h>

static JSClassID system_class, emitter_class;
typedef struct {
    AthenaParticles *system;
    AthenaParticleRenderer *renderer;
    JSValue texture;
    float texture_width, texture_height;
} JSParticleSystem;
typedef struct {
    JSValue owner;
    uint32_t id;
    AthenaParticleEffect effect;
    float rate;
} JSParticleEmitter;

static void release_system(JSParticleSystem *s) {
    if (!s) return;
    athena_particles_renderer_destroy(s->renderer); s->renderer = NULL;
    particles_destroy(s->system); s->system = NULL;
}
static void system_finalizer(JSRuntime *rt, JSValue value) {
    JSParticleSystem *s = JS_GetOpaque(value, system_class);
    if (s) { release_system(s); JS_FreeValueRT(rt, s->texture); js_free_rt(rt, s); }
}
static void system_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark) {
    JSParticleSystem *s = JS_GetOpaque(value, system_class);
    if (s) JS_MarkValue(rt, s->texture, mark);
}
static void emitter_finalizer(JSRuntime *rt, JSValue value) {
    JSParticleEmitter *e = JS_GetOpaque(value, emitter_class);
    if (e) { JS_FreeValueRT(rt, e->owner); js_free_rt(rt, e); }
}
static void emitter_mark(JSRuntime *rt, JSValueConst value, JS_MarkFunc *mark) {
    JSParticleEmitter *e = JS_GetOpaque(value, emitter_class);
    if (e) JS_MarkValue(rt, e->owner, mark);
}
static JSParticleSystem *get_system(JSContext *ctx, JSValueConst obj) {
    JSParticleSystem *s = JS_GetOpaque2(ctx, obj, system_class);
    if (s && !s->system) { JS_ThrowTypeError(ctx, "Particle system is disposed"); return NULL; }
    return s;
}
static JSParticleEmitter *get_emitter(JSContext *ctx, JSValueConst obj, JSParticleSystem **s) {
    JSParticleEmitter *e = JS_GetOpaque2(ctx, obj, emitter_class);
    if (!e) return NULL;
    *s = get_system(ctx, e->owner);
    return *s ? e : NULL;
}
static int number(JSContext *ctx, JSValueConst value, float *out) {
    if (JS_ToFloat32(ctx, out, value) < 0) return -1;
    if (!isfinite(*out)) { JS_ThrowRangeError(ctx, "Particle values must be finite"); return -1; }
    return 0;
}
static int scalar_prop(JSContext *ctx, JSValueConst obj, const char *key, float *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    int status = JS_IsException(v) ? -1 : JS_IsUndefined(v) ? 0 : number(ctx, v, out);
    JS_FreeValue(ctx, v); return status;
}
static int uint_value(JSContext *ctx, JSValueConst v, uint32_t *out, uint32_t min, uint32_t max) {
    double n;
    if (JS_ToFloat64(ctx, &n, v) < 0) return -1;
    if (!isfinite(n) || n < min || n > max || floor(n) != n) {
        JS_ThrowRangeError(ctx, "Particle count/index outside supported range"); return -1;
    }
    *out = (uint32_t)n; return 0;
}
static int uint_prop(JSContext *ctx, JSValueConst obj, const char *key, uint32_t *out, uint32_t min, uint32_t max) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    int result = JS_IsException(v) ? -1 : JS_IsUndefined(v) ? 0 : uint_value(ctx, v, out, min, max);
    JS_FreeValue(ctx, v); return result;
}
static int bool_prop(JSContext *ctx, JSValueConst obj, const char *key, bool *out) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsException(v)) return -1;
    if (!JS_IsUndefined(v)) *out = JS_ToBool(ctx, v);
    JS_FreeValue(ctx, v); return 0;
}
static int components(JSContext *ctx, JSValueConst v, float *out, uint32_t count) {
    JSValue length = JS_GetPropertyStr(ctx, v, "length"); uint32_t n;
    int status = JS_IsException(length) ? -1 : uint_value(ctx, length, &n, count, count);
    JS_FreeValue(ctx, length);
    if (status < 0) return -1;
    for (uint32_t i = 0; i < count; i++) {
        JSValue item = JS_GetPropertyUint32(ctx, v, i);
        int result = JS_IsException(item) ? -1 : number(ctx, item, &out[i]);
        JS_FreeValue(ctx, item); if (result < 0) return -1;
    }
    return 0;
}
static int vector_prop(JSContext *ctx, JSValueConst obj, const char *key, float *out, uint32_t count) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key);
    int result = JS_IsException(v) ? -1 : JS_IsUndefined(v) ? 0 : components(ctx, v, out, count);
    JS_FreeValue(ctx, v); return result;
}
static int range_prop(JSContext *ctx, JSValueConst obj, const char *key, float *min, float *max) {
    JSValue v = JS_GetPropertyStr(ctx, obj, key); int result = 0;
    if (JS_IsException(v)) return -1;
    if (!JS_IsUndefined(v)) {
        if (JS_IsNumber(v)) { result = number(ctx, v, min); *max = *min; }
        else { float pair[2]; result = components(ctx, v, pair, 2); if (!result) { *min = pair[0]; *max = pair[1]; } }
    }
    JS_FreeValue(ctx, v); return result;
}
static int effect_options(JSContext *ctx, JSValueConst opts, AthenaParticleEffect *e, float *rate, const JSParticleSystem *s) {
    if (!JS_IsObject(opts)) { JS_ThrowTypeError(ctx, "Emitter options must be an object"); return -1; }
    if (range_prop(ctx, opts, "lifetime", &e->life_min, &e->life_max) ||
        range_prop(ctx, opts, "size", &e->size_min, &e->size_max) ||
        scalar_prop(ctx, opts, "scaleEnd", &e->scale_end) || scalar_prop(ctx, opts, "aspect", &e->aspect) ||
        scalar_prop(ctx, opts, "drag", &e->drag) || scalar_prop(ctx, opts, "fadeIn", &e->fade_in) ||
        scalar_prop(ctx, opts, "fadeOut", &e->fade_out) || scalar_prop(ctx, opts, "frameRate", &e->frame_rate) ||
        scalar_prop(ctx, opts, "rate", rate) || vector_prop(ctx, opts, "velocityMin", e->velocity_min, 3) ||
        vector_prop(ctx, opts, "velocityMax", e->velocity_max, 3) || vector_prop(ctx, opts, "spread", e->spread, 3) ||
        vector_prop(ctx, opts, "acceleration", e->acceleration, 3) || vector_prop(ctx, opts, "color", e->color, 4) ||
        vector_prop(ctx, opts, "colorEnd", e->color_end, 4) || vector_prop(ctx, opts, "frame", e->frame, 4) ||
        uint_prop(ctx, opts, "frameCount", &e->frame_count, 1, 4096) ||
        uint_prop(ctx, opts, "frameColumns", &e->frame_columns, 1, 4096) || bool_prop(ctx, opts, "frameLoop", &e->frame_loop)) return -1;
    float columns = e->frame_count < e->frame_columns ? e->frame_count : e->frame_columns;
    float rows = (e->frame_count + e->frame_columns - 1) / e->frame_columns;
    if (!particles_effect_valid(e) || *rate < 0 || *rate > 1000000 ||
        e->frame[0] + columns * e->frame[2] > s->texture_width || e->frame[1] + rows * e->frame[3] > s->texture_height) {
        JS_ThrowRangeError(ctx, "Invalid particle effect ranges or atlas frames outside texture"); return -1;
    }
    return 0;
}
static JSValue system_ctor(JSContext *ctx, JSValueConst target, int argc, JSValueConst *argv) {
    if (!argc || !JS_IsObject(argv[0])) return JS_ThrowTypeError(ctx, "Particles.System requires { texture, ... }");
    JSParticleSystem *s = js_mallocz(ctx, sizeof(*s)); if (!s) return JS_EXCEPTION;
    s->texture = JS_UNDEFINED;
    JSValue opts = argv[0], space = JS_UNDEFINED, blend = JS_UNDEFINED, proto = JS_UNDEFINED;
    uint32_t capacity = 512, seed = 42978; bool world = false, sort, depth_test;
    uint64_t equation = GS_ALPHA_BLEND_NORMAL;
    s->texture = JS_GetPropertyStr(ctx, opts, "texture");
    if (JS_IsException(s->texture)) goto fail;
    AthenaImage *image = JS_GetOpaque2(ctx, s->texture, get_img_class_id());
    if (!image) goto fail;
    if (!image->tex || !athena_image_is_loaded(image)) { JS_ThrowTypeError(ctx, "Particle texture must be loaded"); goto fail; }
    s->texture_width = image->tex->Width; s->texture_height = image->tex->Height;
    space = JS_GetPropertyStr(ctx, opts, "space");
    if (JS_IsException(space)) goto fail;
    if (!JS_IsUndefined(space)) {
        const char *str = JS_ToCString(ctx, space); if (!str) goto fail;
        bool valid = !strcmp(str, "world") || !strcmp(str, "screen"); world = !strcmp(str, "world"); JS_FreeCString(ctx, str);
        if (!valid) { JS_ThrowRangeError(ctx, "Particle space must be 'screen' or 'world'"); goto fail; }
    }
    sort = depth_test = world;
    blend = JS_GetPropertyStr(ctx, opts, "blend");
    if (JS_IsException(blend)) goto fail;
    if (!JS_IsUndefined(blend)) {
        const char *str = JS_ToCString(ctx, blend); if (!str) goto fail;
        bool valid = !strcmp(str, "alpha") || !strcmp(str, "additive");
        if (!strcmp(str, "additive")) equation = GS_ALPHA_BLEND_ADD;
        JS_FreeCString(ctx, str);
        if (!valid) { JS_ThrowRangeError(ctx, "Particle blend must be 'alpha' or 'additive'"); goto fail; }
    }
    if (uint_prop(ctx, opts, "capacity", &capacity, 1, ATHENA_PARTICLE_MAX_CAPACITY) ||
        uint_prop(ctx, opts, "seed", &seed, 0, UINT32_MAX) || bool_prop(ctx, opts, "sort", &sort) ||
        bool_prop(ctx, opts, "depthTest", &depth_test)) goto fail;
    // Option getters can run JS (including Image.free); reacquire the image.
    image = JS_GetOpaque2(ctx, s->texture, get_img_class_id());
    if (!image) goto fail;
    if (!image->tex || !athena_image_is_loaded(image)) { JS_ThrowTypeError(ctx, "Particle texture must be loaded"); goto fail; }
    s->texture_width = image->tex->Width; s->texture_height = image->tex->Height;
    s->system = particles_create(capacity, world, sort, seed);
    if (!s->system) { JS_ThrowOutOfMemory(ctx); goto fail; }
    s->renderer = athena_particles_renderer_create(s->system, image, equation, depth_test);
    if (!s->renderer) { JS_ThrowOutOfMemory(ctx); goto fail; }
    proto = JS_GetPropertyStr(ctx, target, "prototype"); if (JS_IsException(proto)) goto fail;
    JSValue obj = JS_NewObjectProtoClass(ctx, proto, system_class);
    if (JS_IsException(obj)) goto fail;
    JS_SetOpaque(obj, s); JS_FreeValue(ctx, proto); JS_FreeValue(ctx, space); JS_FreeValue(ctx, blend); return obj;
fail:
    JS_FreeValue(ctx, proto); JS_FreeValue(ctx, space); JS_FreeValue(ctx, blend);
    release_system(s); JS_FreeValue(ctx, s->texture); js_free(ctx, s); return JS_EXCEPTION;
}
static JSValue create_emitter(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s = get_system(ctx, self); if (!s) return JS_EXCEPTION;
    JSParticleEmitter *e = js_mallocz(ctx, sizeof(*e)); if (!e) return JS_EXCEPTION;
    e->owner = JS_UNDEFINED;
    particles_effect_defaults(&e->effect);
    e->effect.frame[2] = s->texture_width; e->effect.frame[3] = s->texture_height;
    JSValue opts = argc ? argv[0] : JS_UNDEFINED;
    if (!JS_IsUndefined(opts) && effect_options(ctx, opts, &e->effect, &e->rate, s)) goto fail;
    JSValue obj = JS_NewObjectClass(ctx, emitter_class); if (JS_IsException(obj)) goto fail;
    int id = particles_add_emitter(s->system, &e->effect);
    if (id < 0) { JS_FreeValue(ctx, obj); JS_ThrowRangeError(ctx, "Particle emitter limit reached (64 per system)"); goto fail; }
    e->id = id; e->owner = JS_DupValue(ctx, self); JS_SetOpaque(obj, e); return obj;
fail:
    js_free(ctx, e); return JS_EXCEPTION;
}
static JSValue update(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s = get_system(ctx, self); if (!s) return JS_EXCEPTION;
    float dt;
    if (!argc) return JS_ThrowTypeError(ctx, "update(dt) requires seconds");
    if (number(ctx, argv[0], &dt)) return JS_EXCEPTION;
    if (!particles_update(s->system, dt)) return JS_ThrowRangeError(ctx, "Particle dt must be nonnegative");
    return JS_UNDEFINED;
}
static JSValue draw(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s = get_system(ctx, self); if (!s) return JS_EXCEPTION;
    if (!JS_GetOpaque(s->texture, get_img_class_id())) return JS_ThrowTypeError(ctx, "Particle texture is freed");
    return JS_NewUint32(ctx, athena_particles_draw(s->renderer));
}
static JSValue clear(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s = get_system(ctx, self); if (!s) return JS_EXCEPTION;
    uint32_t seed = 0;
    if (argc && uint_value(ctx, argv[0], &seed, 0, UINT32_MAX)) return JS_EXCEPTION;
    particles_clear(s->system, argc != 0, seed); return JS_UNDEFINED;
}
static JSValue dispose(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s = JS_GetOpaque2(ctx, self, system_class); if (!s) return JS_EXCEPTION;
    release_system(s); JS_FreeValue(ctx, s->texture); s->texture = JS_UNDEFINED; return JS_UNDEFINED;
}
static JSValue stats(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s = get_system(ctx, self); if (!s) return JS_EXCEPTION;
    AthenaParticleStats st = particles_stats(s->system); JSValue obj = JS_NewObject(ctx);
    if (JS_IsException(obj)) return obj;
#define PROP(name, val) JS_SetPropertyStr(ctx, obj, name, JS_NewUint32(ctx, val))
    PROP("capacity", st.capacity); PROP("active", st.active); PROP("visible", st.visible);
    PROP("emitted", st.emitted); PROP("dropped", st.dropped); PROP("batches", st.batches);
#undef PROP
    JS_SetPropertyStr(ctx, obj, "sort", JS_NewString(ctx, !st.sorted ? "none" : st.radix ? "radix" : "cached")); return obj;
}
static JSValue count_get(JSContext *ctx, JSValueConst self, int magic) {
    JSParticleSystem *s = get_system(ctx, self); if (!s) return JS_EXCEPTION;
    AthenaParticleStats st = particles_stats(s->system); return JS_NewUint32(ctx, magic ? st.capacity : st.active);
}
static int position_args(JSContext *ctx, int argc, JSValueConst *argv, float p[3]) {
    if (argc < 2) { JS_ThrowTypeError(ctx, "Particle position requires x, y[, z]"); return -1; }
    p[2] = 0;
    return number(ctx, argv[0], &p[0]) || number(ctx, argv[1], &p[1]) || (argc > 2 && number(ctx, argv[2], &p[2])) ? -1 : 0;
}
static JSValue position(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s; JSParticleEmitter *e = get_emitter(ctx, self, &s); if (!e) return JS_EXCEPTION;
    float p[3]; if (position_args(ctx, argc, argv, p)) return JS_EXCEPTION;
    particles_position(s->system, e->id, p); return JS_UNDEFINED;
}
static JSValue burst(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s; JSParticleEmitter *e = get_emitter(ctx, self, &s); if (!e) return JS_EXCEPTION;
    uint32_t count; float p[3];
    if (!argc) return JS_ThrowTypeError(ctx, "burst(count[, x, y, z]) requires count");
    if (uint_value(ctx, argv[0], &count, 0, UINT32_MAX)) return JS_EXCEPTION;
    if (argc > 1 && position_args(ctx, argc - 1, argv + 1, p)) return JS_EXCEPTION;
    return JS_NewUint32(ctx, particles_burst(s->system, e->id, count, argc > 1 ? p : NULL));
}
static JSValue start(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s; JSParticleEmitter *e = get_emitter(ctx, self, &s); if (!e) return JS_EXCEPTION;
    float rate = e->rate;
    if (argc && number(ctx, argv[0], &rate)) return JS_EXCEPTION;
    if (!particles_start(s->system, e->id, rate)) return JS_ThrowRangeError(ctx, "Particle rate must be 0..1000000");
    e->rate = rate; return JS_UNDEFINED;
}
static JSValue stop(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s; JSParticleEmitter *e = get_emitter(ctx, self, &s); if (!e) return JS_EXCEPTION;
    particles_stop(s->system, e->id); return JS_UNDEFINED;
}
static JSValue configure(JSContext *ctx, JSValueConst self, int argc, JSValueConst *argv) {
    JSParticleSystem *s; JSParticleEmitter *e = get_emitter(ctx, self, &s); if (!e) return JS_EXCEPTION;
    if (!argc) return JS_ThrowTypeError(ctx, "configure(options) requires options");
    AthenaParticleEffect effect = e->effect; float rate = e->rate;
    if (effect_options(ctx, argv[0], &effect, &rate, s)) return JS_EXCEPTION;
    particles_configure(s->system, e->id, &effect); e->effect = effect; e->rate = rate;
    // configure(rate) changes the next start(); stop/start explicitly controls emission.
    return JS_UNDEFINED;
}
static const JSCFunctionListEntry system_methods[] = {
    JS_CFUNC_DEF("createEmitter", 1, create_emitter), JS_CFUNC_DEF("update", 1, update),
    JS_CFUNC_DEF("draw", 0, draw), JS_CFUNC_DEF("clear", 0, clear), JS_CFUNC_DEF("dispose", 0, dispose), JS_CFUNC_DEF("stats", 0, stats),
    JS_CGETSET_MAGIC_DEF("activeCount", count_get, NULL, 0), JS_CGETSET_MAGIC_DEF("capacity", count_get, NULL, 1)
};
static const JSCFunctionListEntry emitter_methods[] = {
    JS_CFUNC_DEF("setPosition", 3, position), JS_CFUNC_DEF("burst", 4, burst),
    JS_CFUNC_DEF("start", 1, start), JS_CFUNC_DEF("stop", 0, stop), JS_CFUNC_DEF("configure", 1, configure)
};
static int module_init(JSContext *ctx, JSModuleDef *m) {
    JS_NewClassID(&system_class); JS_NewClassID(&emitter_class);
    JSClassDef sys_def = { .class_name = "ParticleSystem", .finalizer = system_finalizer, .gc_mark = system_mark };
    JSClassDef em_def = { .class_name = "ParticleEmitter", .finalizer = emitter_finalizer, .gc_mark = emitter_mark };
    if (JS_NewClass(JS_GetRuntime(ctx), system_class, &sys_def) < 0 || JS_NewClass(JS_GetRuntime(ctx), emitter_class, &em_def) < 0) return -1;
    JSValue proto = JS_NewObject(ctx), em_proto = JS_NewObject(ctx);
    if (JS_IsException(proto) || JS_IsException(em_proto)) { JS_FreeValue(ctx, proto); JS_FreeValue(ctx, em_proto); return -1; }
    JS_SetPropertyFunctionList(ctx, proto, system_methods, countof(system_methods));
    JS_SetPropertyFunctionList(ctx, em_proto, emitter_methods, countof(emitter_methods));
    JSValue ctor = JS_NewCFunction2(ctx, system_ctor, "System", 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto); JS_SetClassProto(ctx, system_class, proto); JS_SetClassProto(ctx, emitter_class, em_proto);
    return JS_SetModuleExport(ctx, m, "System", ctor);
}
JSModuleDef *athena_particles_init(JSContext *ctx) {
    JSModuleDef *m = JS_NewCModule(ctx, "Particles", module_init);
    if (m) JS_AddModuleExport(ctx, m, "System");
    return m;
}
