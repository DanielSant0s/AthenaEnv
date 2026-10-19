#include <ath_env.h>
#include <athena/screen.h>
#include <athena/image.h>
#include <ath_bindings.h>

static JSValue js_screen_set_clear_color(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	Color clear_color = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
  	JS_ToInt32(ctx, &clear_color, argv[0]);
  	athena_screen_set_clear_color(clear_color);
  	return JS_UNDEFINED;
}

static JSValue js_screen_display(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	js_set_render_loop_func(ctx, JS_DupValue(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue js_screen_flip(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
  athena_screen_flip();
  return JS_UNDEFINED;
}

static JSValue js_screen_clear(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	Color color = GS_SETREG_RGBAQ(0x00, 0x00, 0x00, 0x80, 0x00);
  	if (argc == 1) JS_ToInt32(ctx, &color, argv[0]);
	athena_screen_clear(color);
	return JS_UNDEFINED;
}

static JSValue js_screen_vblank(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	athena_screen_wait_vblank();
	return JS_UNDEFINED;
}

static JSValue js_screen_vsync(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	athena_screen_set_vsync(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue js_screen_fcount(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	athena_screen_set_frame_counter(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue js_screen_set_frame_overlap(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	athena_screen_set_frame_overlap(JS_ToBool(ctx, argv[0]));
	return JS_UNDEFINED;
}

static JSValue js_screen_get_frame_overlap(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	return JS_NewBool(ctx, athena_screen_get_frame_overlap());
}

// Screen.getProfile(): EE time per frame, in ms, averaged over the last 30
// frames. gpuWait is time the EE sat blocked on the GPU -- near zero means the
// EE is the bottleneck, large means the GPU is.
static JSValue js_screen_get_profile(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	const prof_report *r = athena_screen_get_profile();
	JSValue obj = JS_NewObject(ctx);

	JS_DefinePropertyValueStr(ctx, obj, "frame",   JS_NewFloat64(ctx, r->frame_ms), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "script",  JS_NewFloat64(ctx, r->section_ms[PROF_SCRIPT]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "render",  JS_NewFloat64(ctx, r->section_ms[PROF_RENDER]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "anim",    JS_NewFloat64(ctx, r->section_ms[PROF_ANIM]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "physics", JS_NewFloat64(ctx, r->section_ms[PROF_PHYSICS]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "gpuWait", JS_NewFloat64(ctx, r->section_ms[PROF_GPU_WAIT]), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "vsync",   JS_NewFloat64(ctx, r->section_ms[PROF_VSYNC]), JS_PROP_C_W_E);

	return obj;
}

static JSValue js_screen_get_free_vram(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	uint32_t mode = VRAM_USED_TOTAL;
	if (argc)
		JS_ToUint32(ctx, &mode, argv[0]);
	return JS_NewUint32(ctx, (uint32_t)athena_screen_get_free_vram(mode));
}

static JSValue js_screen_get_fps(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	/* the interval is also the sampling period; 0 would stop the counter */
	int time_intvl = 60;
	if (argc > 0 && !JS_IsUndefined(argv[0]))
		JS_ToInt32(ctx, &time_intvl, argv[0]);
	return JS_NewFloat32(ctx, athena_screen_get_fps(time_intvl));
}

static JSValue js_screen_get_mode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	AthenaVideoMode mode;
	JSValue obj = JS_NewObject(ctx);
	athena_screen_get_mode(&mode);
	JS_DefinePropertyValueStr(ctx, obj, "mode", JS_NewInt32(ctx, mode.mode), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "width", JS_NewInt32(ctx, mode.width), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "height", JS_NewInt32(ctx, mode.height), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "psm", JS_NewInt32(ctx, mode.psm), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "interlace", JS_NewInt32(ctx, mode.interlace), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "field", JS_NewInt32(ctx, mode.field), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "psmz", JS_NewInt32(ctx, mode.psmz), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "zbuffering", JS_NewBool(ctx, mode.zbuffering), JS_PROP_C_W_E);
	JS_DefinePropertyValueStr(ctx, obj, "double_buffering", JS_NewBool(ctx, mode.double_buffering), JS_PROP_C_W_E);
	return obj;
}

static JSValue js_screen_set_mode(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	AthenaVideoMode mode = {0};
	JSValue val;

	val = JS_GetPropertyStr(ctx, argv[0], "mode");
	JS_ToInt32(ctx, &mode.mode, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "width");
	JS_ToInt32(ctx, &mode.width, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "height");
	JS_ToInt32(ctx, &mode.height, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "psm");
	JS_ToInt32(ctx, &mode.psm, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "interlace");
	JS_ToInt32(ctx, &mode.interlace, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "field");
	JS_ToInt32(ctx, &mode.field, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "psmz");
	JS_ToInt32(ctx, &mode.psmz, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "zbuffering");
	mode.zbuffering = JS_ToBool(ctx, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "double_buffering");
	mode.double_buffering = JS_ToBool(ctx, val);
	JS_FreeValue(ctx, val);
	val = JS_GetPropertyStr(ctx, argv[0], "pass_count");
	JS_ToUint32(ctx, &mode.pass_count, val);
	JS_FreeValue(ctx, val);

	athena_screen_set_mode(&mode);
	return JS_UNDEFINED;
}

static JSValue js_screen_alpha_equation(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	int a, b, c, d, fix;
	JS_ToInt32(ctx, &a, argv[0]);
	JS_ToInt32(ctx, &b, argv[1]);
	JS_ToInt32(ctx, &c, argv[2]);
	JS_ToInt32(ctx, &d, argv[3]);
	JS_ToInt32(ctx, &fix, argv[4]);
	return JS_NewInt64(ctx, athena_screen_alpha_equation(a, b, c, d, fix));
}

static JSValue js_screen_set_param(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	int param;
	uint64_t value;
	JS_ToInt32(ctx, &param, argv[0]);

	switch (param) {
		case ALPHA_TEST_ENABLE:
		case DST_ALPHA_TEST_ENABLE:
		case DEPTH_TEST_ENABLE:
		case PIXEL_ALPHA_BLEND_ENABLE:
			value = (uint64_t)JS_ToBool(ctx, argv[1]);
			break;
		case ALPHA_TEST_METHOD:
		case ALPHA_TEST_REF:
		case ALPHA_TEST_FAIL:
		case DST_ALPHA_TEST_METHOD:
		case DEPTH_TEST_METHOD:
		case COLOR_CLAMP_MODE:
			JS_ToInt64(ctx, &value, argv[1]);
			break;
		case ALPHA_BLEND_EQUATION:
			if (JS_IsNumber(argv[1]) || JS_IsBigInt(ctx, argv[1])) {
				JS_ToInt64(ctx, (int64_t *)&value, argv[1]);
			} else {
				int a = 0, b = 1, c = 0, d = 1, fix = 0;
				JSValue va = JS_GetPropertyStr(ctx, argv[1], "a");
				JSValue vb = JS_GetPropertyStr(ctx, argv[1], "b");
				JSValue vc = JS_GetPropertyStr(ctx, argv[1], "c");
				JSValue vd = JS_GetPropertyStr(ctx, argv[1], "d");
				JSValue vfix = JS_GetPropertyStr(ctx, argv[1], "fix");
				JS_ToInt32(ctx, &a, va);
				JS_ToInt32(ctx, &b, vb);
				JS_ToInt32(ctx, &c, vc);
				JS_ToInt32(ctx, &d, vd);
				JS_ToInt32(ctx, &fix, vfix);
				JS_FreeValue(ctx, va);
				JS_FreeValue(ctx, vb);
				JS_FreeValue(ctx, vc);
				JS_FreeValue(ctx, vd);
				JS_FreeValue(ctx, vfix);
				value = athena_screen_alpha_equation(a, b, c, d, fix);
			}
			break;
		case SCISSOR_BOUNDS:
			if (JS_IsNumber(argv[1])) {
				JS_ToInt64(ctx, &value, argv[1]);
			} else {
				int x0, x1, y0, y1;
				JS_ToInt32(ctx, &x0, JS_GetPropertyStr(ctx, argv[1], "x1"));
				JS_ToInt32(ctx, &y0, JS_GetPropertyStr(ctx, argv[1], "y1"));
				JS_ToInt32(ctx, &x1, JS_GetPropertyStr(ctx, argv[1], "x2"));
				JS_ToInt32(ctx, &y1, JS_GetPropertyStr(ctx, argv[1], "y2"));
				value = GS_SETREG_SCISSOR_1(x0, x1, y0, y1);
			}
			break;
		default:
			return JS_UNDEFINED;
	}

	athena_screen_set_param(param, value);
	return JS_UNDEFINED;
}

static JSValue js_screen_get_param(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	int param;
	JS_ToInt32(ctx, &param, argv[0]);
	uint64_t value = athena_screen_get_param(param);

	switch (param) {
		case ALPHA_TEST_ENABLE:
		case ALPHA_TEST_METHOD:
		case ALPHA_TEST_REF:
		case ALPHA_TEST_FAIL:
		case DST_ALPHA_TEST_ENABLE:
		case DST_ALPHA_TEST_METHOD:
		case DEPTH_TEST_ENABLE:
		case DEPTH_TEST_METHOD:
		case PIXEL_ALPHA_BLEND_ENABLE:
		case COLOR_CLAMP_MODE:
			return JS_NewInt32(ctx, value);
		case ALPHA_BLEND_EQUATION:
			{
				AthenaAlphaBlend blend;
				JSValue obj = JS_NewObject(ctx);
				if (!athena_screen_get_alpha_blend(param, &blend))
					return JS_UNDEFINED;
				JS_DefinePropertyValueStr(ctx, obj, "a", JS_NewInt32(ctx, blend.a), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "b", JS_NewInt32(ctx, blend.b), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "c", JS_NewInt32(ctx, blend.c), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "d", JS_NewInt32(ctx, blend.d), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "fix", JS_NewInt32(ctx, blend.fix), JS_PROP_C_W_E);
				return obj;
			}
		case SCISSOR_BOUNDS:
			{
				AthenaScissorBounds bounds;
				JSValue obj = JS_NewObject(ctx);
				if (!athena_screen_get_scissor_bounds(param, &bounds))
					return JS_UNDEFINED;
				JS_DefinePropertyValueStr(ctx, obj, "x1", JS_NewInt32(ctx, bounds.x1), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "y1", JS_NewInt32(ctx, bounds.y1), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "x2", JS_NewInt32(ctx, bounds.x2), JS_PROP_C_W_E);
				JS_DefinePropertyValueStr(ctx, obj, "y2", JS_NewInt32(ctx, bounds.y2), JS_PROP_C_W_E);
				return obj;
			}
	}
	return JS_UNDEFINED;
}

JSValue js_screen_buffers[3] = { JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED };
JSValue js_current_screen_buffers[3] = { JS_UNDEFINED, JS_UNDEFINED, JS_UNDEFINED };

static JSValue js_screen_init_buffers(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (athena_screen_buffers_ready() && !JS_IsUndefined(js_screen_buffers[0]))
		return JS_UNDEFINED;

	if (athena_screen_init_buffers() < 0)
		return JS_EXCEPTION;

	for (int i = 0; i < 3; i++) {
		AthenaImage *image = athena_screen_get_buffer(i);
		js_screen_buffers[i] = JS_NewObjectClass(ctx, get_img_class_id());
		if (JS_IsException(js_screen_buffers[i]))
			return JS_EXCEPTION;
		JS_SetOpaque(js_screen_buffers[i], image);
		js_current_screen_buffers[i] = js_screen_buffers[i];
	}
	return JS_UNDEFINED;
}

static JSValue js_screen_get_buffer(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (!athena_screen_buffers_ready())
		return JS_ThrowInternalError(ctx, "Call Screen.initBuffers() before using this function!");
	int buffer_id;
	JS_ToInt32(ctx, &buffer_id, argv[0]);
	return JS_DupValue(ctx, js_current_screen_buffers[buffer_id]);
}

static JSValue js_screen_set_buffer(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	int buffer_id;
	uint32_t mask = 0;

	if (!athena_screen_buffers_ready())
		return JS_ThrowInternalError(ctx, "Call Screen.initBuffers() before using this function!");

	JS_ToInt32(ctx, &buffer_id, argv[0]);
	AthenaImage *image = JS_GetOpaque2(ctx, argv[1], get_img_class_id());
	if (argc > 2)
		JS_ToUint32(ctx, &mask, argv[2]);

	athena_screen_set_buffer(buffer_id, image, mask);

	if (!getGSGLOBAL()->PrimContext)
		js_current_screen_buffers[buffer_id] = argv[1];

	return JS_UNDEFINED;
}

static JSValue js_screen_reset_buffers(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (!athena_screen_buffers_ready())
		return JS_ThrowInternalError(ctx, "Call Screen.initBuffers() before using this function!");
	athena_screen_reset_buffers();
	for (int i = 0; i < 3; i++)
		js_current_screen_buffers[i] = js_screen_buffers[i];
	return JS_UNDEFINED;
}

static JSValue js_screen_switch_context(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	return JS_NewInt32(ctx, athena_screen_switch_context());
}

static JSValue js_screen_flush(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	athena_screen_flush();
	return JS_UNDEFINED;
}

static int js_parse_lut_entries(JSContext *ctx, JSValueConst arg, uint32_t out_entries[256]) {
	if (JS_IsUndefined(arg) || JS_IsNull(arg)) {
		for (uint32_t i = 0; i < 256; i++) {
			out_entries[i] = i | (i << 8) | (i << 16) | (0x80u << 24);
		}
		return 0;
	}

	if (JS_IsFunction(ctx, arg)) {
		for (uint32_t i = 0; i < 256; i++) {
			JSValue idx_val = JS_NewUint32(ctx, i);
			JSValue ret = JS_Call(ctx, arg, JS_UNDEFINED, 1, &idx_val);
			JS_FreeValue(ctx, idx_val);
			if (JS_IsException(ret))
				return -1;
			uint32_t col = 0;
			JS_ToUint32(ctx, &col, ret);
			JS_FreeValue(ctx, ret);
			out_entries[i] = col;
		}
		return 0;
	}

	size_t byte_offset = 0, byte_len = 0, bytes_per_elem = 0;
	JSValue abuf = JS_GetTypedArrayBuffer(ctx, arg, &byte_offset, &byte_len, &bytes_per_elem);
	if (!JS_IsException(abuf)) {
		size_t total_size = 0;
		uint8_t *raw = JS_GetArrayBuffer(ctx, &total_size, abuf);
		if (raw && (byte_offset + byte_len <= total_size) && byte_len >= 256 * sizeof(uint32_t)) {
			memcpy(out_entries, raw + byte_offset, 256 * sizeof(uint32_t));
			JS_FreeValue(ctx, abuf);
			return 0;
		}
		JS_FreeValue(ctx, abuf);
	} else {
		JS_FreeValue(ctx, JS_GetException(ctx));
	}

	if (JS_IsObject(arg)) {
		for (uint32_t i = 0; i < 256; i++) {
			JSValue elem = JS_GetPropertyUint32(ctx, arg, i);
			if (JS_IsException(elem))
				return -1;
			uint32_t col = 0;
			if (!JS_IsUndefined(elem))
				JS_ToUint32(ctx, &col, elem);
			JS_FreeValue(ctx, elem);
			out_entries[i] = col;
		}
		return 0;
	}

	JS_ThrowTypeError(ctx, "Expected a 256-entry array, Uint32Array, or generator function(i)");
	return -1;
}

static JSValue js_screen_create_shuffle_lut(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	uint32_t entries[256];
	const uint32_t *entries_ptr = NULL;

	if (argc >= 1 && !JS_IsUndefined(argv[0]) && !JS_IsNull(argv[0])) {
		if (js_parse_lut_entries(ctx, argv[0], entries) < 0)
			return JS_EXCEPTION;
		entries_ptr = entries;
	}

	AthenaImage *lut = athena_screen_create_shuffle_lut(entries_ptr);
	if (!lut)
		return JS_ThrowOutOfMemory(ctx);

	JSValue obj = JS_NewObjectClass(ctx, get_img_class_id());
	if (JS_IsException(obj)) {
		athena_image_destroy(lut);
		return JS_EXCEPTION;
	}
	JS_SetOpaque(obj, lut);
	return obj;
}

static JSValue js_screen_update_shuffle_lut(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_ThrowTypeError(ctx, "Screen.updateShuffleLUT(lut, entriesOrFn) requires 2 arguments");

	AthenaImage *lut = JS_GetOpaque2(ctx, argv[0], get_img_class_id());
	if (!lut)
		return JS_EXCEPTION;

	uint32_t entries[256];
	if (js_parse_lut_entries(ctx, argv[1], entries) < 0)
		return JS_EXCEPTION;

	athena_screen_update_shuffle_lut(lut, entries);
	return JS_UNDEFINED;
}

static void js_parse_depth_fog_options(JSContext *ctx, JSValueConst opts, gs_depth_fog_config *cfg) {
	if (!JS_IsObject(opts))
		return;

	JSValue v;
	double dval;
	int32_t ival;

	v = JS_GetPropertyStr(ctx, opts, "near");
	if (!JS_IsUndefined(v)) {
		if (!JS_ToFloat64(ctx, &dval, v)) cfg->near_z = (float)dval;
	}
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, opts, "far");
	if (!JS_IsUndefined(v)) {
		if (!JS_ToFloat64(ctx, &dval, v)) cfg->far_z = (float)dval;
	}
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, opts, "density");
	if (!JS_IsUndefined(v)) {
		if (!JS_ToFloat64(ctx, &dval, v)) cfg->density = (float)dval;
	}
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, opts, "skyFog");
	if (!JS_IsUndefined(v)) {
		if (!JS_ToFloat64(ctx, &dval, v)) cfg->sky_fog = (float)dval;
	}
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, opts, "curve");
	if (!JS_IsUndefined(v)) {
		if (!JS_ToInt32(ctx, &ival, v)) cfg->curve = (eFogCurve)ival;
	}
	JS_FreeValue(ctx, v);

	v = JS_GetPropertyStr(ctx, opts, "shift");
	if (!JS_IsUndefined(v)) {
		if (!JS_ToInt32(ctx, &ival, v)) cfg->shift = ival;
	}
	JS_FreeValue(ctx, v);
}

static JSValue js_screen_build_depth_fog_lut(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc < 4)
		return JS_ThrowTypeError(ctx, "Screen.buildDepthFogLUT(lut, color, start, end, options?) requires at least 4 arguments");

	AthenaImage *lut = JS_GetOpaque2(ctx, argv[0], get_img_class_id());
	if (!lut)
		return JS_EXCEPTION;

	uint32_t color = 0x80808080u;
	double start = 50.0, end = 500.0;
	JS_ToUint32(ctx, &color, argv[1]);
	JS_ToFloat64(ctx, &start, argv[2]);
	JS_ToFloat64(ctx, &end, argv[3]);

	gs_depth_fog_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.color   = color;
	cfg.start   = (float)start;
	cfg.end     = (float)end;
	cfg.density = 1.5f;
	cfg.sky_fog = 0.0f;
	cfg.curve   = FOG_CURVE_LINEAR;

	if (argc >= 5) {
		js_parse_depth_fog_options(ctx, argv[4], &cfg);
	}

	athena_screen_build_depth_fog_lut(lut, &cfg);
	return JS_UNDEFINED;
}

static GSSURFACE *js_resolve_shuffle_surface(JSContext *ctx, JSValueConst val, GSSURFACE *default_surf) {
	if (JS_IsUndefined(val) || JS_IsNull(val))
		return default_surf;

	if (JS_IsNumber(val)) {
		int32_t buf_id = -1;
		JS_ToInt32(ctx, &buf_id, val);
		if (buf_id >= 0 && buf_id <= 2) {
			return main_screen_buffer[buf_id];
		}
		return default_surf;
	}

	AthenaImage *img = JS_GetOpaque(val, get_img_class_id());
	if (img && img->tex)
		return img->tex;

	return default_surf;
}

static JSValue js_screen_channel_shuffle(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc < 2)
		return JS_ThrowTypeError(ctx, "Screen.channelShuffle(source, lut, inChannel?, options?) requires at least 2 arguments");

	GSSURFACE *src_surf = js_resolve_shuffle_surface(ctx, argv[0], main_screen_buffer[DEPTH_BUFFER]);
	AthenaImage *lut = JS_GetOpaque2(ctx, argv[1], get_img_class_id());
	if (!lut)
		return JS_EXCEPTION;

	gs_channel_shuffle_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.in_channel     = CHANNEL_SHUFFLE_BLUE;
	cfg.out_mask       = CHANNEL_SHUFFLE_RGB;
	cfg.alpha_blend    = true;
	cfg.blend_equation = GS_ALPHA_BLEND_NORMAL;
	cfg.alpha_test     = true;
	cfg.z_test_enable  = false;
	cfg.z_test_method  = DEPTH_ALWAYS;

	if (argc >= 3 && JS_IsNumber(argv[2])) {
		JS_ToUint32(ctx, &cfg.in_channel, argv[2]);
	}

	GSSURFACE *dst_surf = main_screen_buffer[DRAW_BUFFER];
	GSSURFACE raw_source, raw_destination;
	JSValueConst opts = JS_UNDEFINED;
	if (argc >= 3 && JS_IsObject(argv[2])) {
		opts = argv[2];
	} else if (argc >= 4 && JS_IsObject(argv[3])) {
		opts = argv[3];
	}

	if (JS_IsObject(opts)) {
		JSValue v;

		v = JS_GetPropertyStr(ctx, opts, "dst");
		if (!JS_IsUndefined(v)) {
			dst_surf = js_resolve_shuffle_surface(ctx, v, dst_surf);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "inChannel");
		if (!JS_IsUndefined(v)) {
			JS_ToUint32(ctx, &cfg.in_channel, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "outMask");
		if (!JS_IsUndefined(v)) {
			JS_ToUint32(ctx, &cfg.out_mask, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "alphaBlend");
		if (!JS_IsUndefined(v)) {
			cfg.alpha_blend = JS_ToBool(ctx, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "blendEquation");
		if (!JS_IsUndefined(v)) {
			JS_ToIndex(ctx, &cfg.blend_equation, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "alphaTest");
		if (!JS_IsUndefined(v)) {
			cfg.alpha_test = JS_ToBool(ctx, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "zTest");
		if (!JS_IsUndefined(v)) {
			cfg.z_test_enable = JS_ToBool(ctx, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "zTestMethod");
		if (!JS_IsUndefined(v)) {
			uint32_t zm = DEPTH_ALWAYS;
			JS_ToUint32(ctx, &zm, v);
			cfg.z_test_method = (uint8_t)zm;
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "zRef");
		if (!JS_IsUndefined(v)) {
			JS_ToUint32(ctx, &cfg.z_ref, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "x");
		if (!JS_IsUndefined(v)) {
			JS_ToUint32(ctx, &cfg.x, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "y");
		if (!JS_IsUndefined(v)) {
			JS_ToUint32(ctx, &cfg.y, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "width");
		if (!JS_IsUndefined(v)) {
			JS_ToUint32(ctx, &cfg.width, v);
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "height");
		if (!JS_IsUndefined(v)) {
			JS_ToUint32(ctx, &cfg.height, v);
		}
		JS_FreeValue(ctx, v);

		// Stack-only views: reinterpret physical bytes without changing the
		// Image or allocating VRAM. In particular, CT32 aliases can access the
		// unused alpha byte of Z24 while leaving its depth bits intact.
		v = JS_GetPropertyStr(ctx, opts, "rawSource");
		if (JS_ToBool(ctx, v)) {
			raw_source = *src_surf;
			raw_source.PSM = GS_PSM_CT32;
			src_surf = &raw_source;
		}
		JS_FreeValue(ctx, v);

		v = JS_GetPropertyStr(ctx, opts, "rawDestination");
		if (JS_ToBool(ctx, v)) {
			raw_destination = *dst_surf;
			raw_destination.PSM = GS_PSM_CT32;
			dst_surf = &raw_destination;
		}
		JS_FreeValue(ctx, v);
	}

	gs_channel_shuffle_ex(dst_surf, src_surf, lut->tex, &cfg);
	return JS_UNDEFINED;
}

static JSValue js_screen_depth_fog(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	if (argc < 3)
		return JS_ThrowTypeError(ctx, "Screen.depthFog(color, start, end, options?) requires at least 3 arguments");

	uint32_t color = 0x80808080u;
	double start = 50.0, end = 500.0;
	JS_ToUint32(ctx, &color, argv[0]);
	JS_ToFloat64(ctx, &start, argv[1]);
	JS_ToFloat64(ctx, &end, argv[2]);

	gs_depth_fog_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.color   = color;
	cfg.start   = (float)start;
	cfg.end     = (float)end;
	cfg.density = 1.5f;
	cfg.sky_fog = 0.0f;
	cfg.curve   = FOG_CURVE_LINEAR;

	if (argc >= 4) {
		js_parse_depth_fog_options(ctx, argv[3], &cfg);
	}

	athena_screen_depth_fog(&cfg);
	return JS_UNDEFINED;
}

static JSValue js_screen_depth_bitshift(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	int32_t shift = 1;
	if (argc >= 1)
		JS_ToInt32(ctx, &shift, argv[0]);
	gs_depth_bitshift(shift);
	return JS_UNDEFINED;
}

static JSValue js_screen_depth_invert_blue(JSContext *ctx, JSValue this_val, int argc, JSValueConst *argv) {
	(void)ctx; (void)this_val; (void)argc; (void)argv;
	gs_depth_invert_blue();
	return JS_UNDEFINED;
}

static const JSCFunctionListEntry module_funcs[] = {
    JS_CFUNC_DEF("flip", 0, js_screen_flip),
    JS_CFUNC_DEF("clear", 1, js_screen_clear),
	JS_CFUNC_DEF("getMemoryStats", 1, js_screen_get_free_vram),
	JS_PROP_INT32_DEF("VRAM_SIZE", VRAM_SIZE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("VRAM_USED_TOTAL", VRAM_USED_TOTAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("VRAM_USED_STATIC", VRAM_USED_STATIC, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("VRAM_USED_DYNAMIC", VRAM_USED_DYNAMIC, JS_PROP_CONFIGURABLE),
	JS_CFUNC_DEF("getFPS", 1, js_screen_get_fps),
    JS_CFUNC_DEF("waitVblankStart", 0, js_screen_vblank),
	JS_CFUNC_DEF("setVSync", 1, js_screen_vsync),
	JS_CFUNC_DEF("setFrameCounter", 1, js_screen_fcount),
	JS_CFUNC_DEF("setFrameOverlap", 1, js_screen_set_frame_overlap),
	JS_CFUNC_DEF("getFrameOverlap", 0, js_screen_get_frame_overlap),
	JS_CFUNC_DEF("getProfile", 0, js_screen_get_profile),
	JS_CFUNC_DEF("getMode", 0, js_screen_get_mode),
    JS_CFUNC_DEF("setMode", 1, js_screen_set_mode),
	JS_CFUNC_DEF("clearColor", 1, js_screen_set_clear_color),
	JS_CFUNC_DEF("display", 1, js_screen_display),
	JS_CFUNC_DEF("getParam", 1, js_screen_get_param),
	JS_CFUNC_DEF("setParam", 2, js_screen_set_param),
	JS_CFUNC_DEF("alphaEquation", 1, js_screen_alpha_equation),
	JS_PROP_INT32_DEF("ALPHA_TEST_ENABLE", ALPHA_TEST_ENABLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_TEST_METHOD", ALPHA_TEST_METHOD, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_TEST_REF", ALPHA_TEST_REF, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_TEST_FAIL", ALPHA_TEST_FAIL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_NEVER", ALPHA_NEVER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_ALWAYS", ALPHA_ALWAYS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_LESS", ALPHA_LESS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_LEQUAL", ALPHA_LEQUAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_EQUAL", ALPHA_EQUAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_GEQUAL", ALPHA_GEQUAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_GREATER", ALPHA_GREATER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_NEQUAL", ALPHA_NEQUAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_FAIL_NO_UPDATE", ALPHA_FAIL_NO_UPDATE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_FAIL_FB_ONLY", ALPHA_FAIL_FB_ONLY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_FAIL_ZB_ONLY", ALPHA_FAIL_ZB_ONLY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_FAIL_RGB_ONLY", ALPHA_FAIL_RGB_ONLY, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DST_ALPHA_TEST_ENABLE", DST_ALPHA_TEST_ENABLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DST_ALPHA_TEST_METHOD", DST_ALPHA_TEST_METHOD, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DST_ALPHA_ZERO", DEST_ALPHA_ZERO, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DST_ALPHA_ONE", DEST_ALPHA_ONE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DEPTH_TEST_ENABLE", DEPTH_TEST_ENABLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DEPTH_TEST_METHOD", DEPTH_TEST_METHOD, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DEPTH_NEVER", DEPTH_NEVER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DEPTH_ALWAYS", DEPTH_ALWAYS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DEPTH_GEQUAL", DEPTH_GEQUAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DEPTH_GREATER", DEPTH_GREATER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_BLEND_EQUATION", ALPHA_BLEND_EQUATION, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SRC_RGB", SRC_RGB, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DST_RGB", DST_RGB, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ZERO_RGB", ZERO_RGB, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SRC_ALPHA", SRC_ALPHA, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DST_ALPHA", DST_ALPHA, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("ALPHA_FIX", ALPHA_FIX, JS_PROP_CONFIGURABLE),
	JS_PROP_INT64_DEF("BLEND_DEFAULT", GS_ALPHA_BLEND_NORMAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT64_DEF("BLEND_ADD_NOALPHA", GS_ALPHA_BLEND_ADD_NOALPHA, JS_PROP_CONFIGURABLE),
	JS_PROP_INT64_DEF("BLEND_ADD", GS_ALPHA_BLEND_ADD, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("SCISSOR_BOUNDS", SCISSOR_BOUNDS, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PIXEL_ALPHA_BLEND_ENABLE", PIXEL_ALPHA_BLEND_ENABLE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("COLOR_CLAMP_MODE", COLOR_CLAMP_MODE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("NTSC", GS_MODE_NTSC, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_480p", GS_MODE_DTV_480P, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PAL", GS_MODE_PAL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_576p", GS_MODE_DTV_576P, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_720p", GS_MODE_DTV_720P, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DTV_1080i", GS_MODE_DTV_1080I, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("INTERLACED", GS_INTERLACED, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("PROGRESSIVE", GS_NONINTERLACED, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FIELD", GS_FIELD, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FRAME", GS_FRAME, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT32", GS_PSM_CT32, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT24", GS_PSM_CT24, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT16", GS_PSM_CT16, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CT16S", GS_PSM_CT16S, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z32", GS_ZBUF_32, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z24", GS_ZBUF_24, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z16", GS_ZBUF_16, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("Z16S", GS_ZBUF_16S, JS_PROP_CONFIGURABLE),
	JS_CFUNC_DEF("initBuffers", 0, js_screen_init_buffers),
	JS_CFUNC_DEF("resetBuffers", 0, js_screen_reset_buffers),
	JS_CFUNC_DEF("getBuffer", 1, js_screen_get_buffer),
	JS_CFUNC_DEF("setBuffer", 3, js_screen_set_buffer),
	JS_PROP_INT32_DEF("DRAW_BUFFER", DRAW_BUFFER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DISPLAY_BUFFER", DISPLAY_BUFFER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("DEPTH_BUFFER", DEPTH_BUFFER, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CHANNEL_RED", CHANNEL_SHUFFLE_RED, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CHANNEL_GREEN", CHANNEL_SHUFFLE_GREEN, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CHANNEL_BLUE", CHANNEL_SHUFFLE_BLUE, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CHANNEL_ALPHA", CHANNEL_SHUFFLE_ALPHA, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CHANNEL_RGB", CHANNEL_SHUFFLE_RGB, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("CHANNEL_ALL", (int32_t)CHANNEL_SHUFFLE_ALL, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FOG_LINEAR", FOG_CURVE_LINEAR, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FOG_EXP", FOG_CURVE_EXP, JS_PROP_CONFIGURABLE),
	JS_PROP_INT32_DEF("FOG_EXP2", FOG_CURVE_EXP2, JS_PROP_CONFIGURABLE),
	JS_CFUNC_DEF("createShuffleLUT", 1, js_screen_create_shuffle_lut),
	JS_CFUNC_DEF("updateShuffleLUT", 2, js_screen_update_shuffle_lut),
	JS_CFUNC_DEF("buildDepthFogLUT", 5, js_screen_build_depth_fog_lut),
	JS_CFUNC_DEF("channelShuffle", 4, js_screen_channel_shuffle),
	JS_CFUNC_DEF("depthFog", 4, js_screen_depth_fog),
	JS_CFUNC_DEF("depthBitShift", 1, js_screen_depth_bitshift),
	JS_CFUNC_DEF("depthInvertBlue", 0, js_screen_depth_invert_blue),
	JS_CFUNC_DEF("switchContext", 0, js_screen_switch_context),
	JS_CFUNC_DEF("flush", 0, js_screen_flush),
};

static int js_screen_module_init(JSContext *ctx, JSModuleDef *m) {
    return JS_SetModuleExportList(ctx, m, module_funcs, countof(module_funcs));
}

JSModuleDef *athena_screen_init(JSContext* ctx){
	return athena_push_module(ctx, js_screen_module_init, module_funcs, countof(module_funcs), "Screen");
}
