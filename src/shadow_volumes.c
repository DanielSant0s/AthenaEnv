// Stencil shadow volumes -- see shadow_volumes.h.
//
// Per frame:
//   begin   red of the region -> the top byte of Z24 (saved), red cleared
//   render  each volume: front faces red += 1, back faces red -= 1, Z tested,
//           not written
//   end     red -> alpha through a LUT (0 lit, anything else shadowed); red
//           restored times that alpha; green and blue times it too. Or, the
//           relight end: red -> the alpha bit, red restored, and the objects
//           drawn again ambient only where the bit is set (destination alpha
//           test)
//
// The volume is the caster's silhouette pushed away from the light (along a
// directional light, radially from a point light), worked out on the EE: per
// triangle, does it face the light; per edge, do its two triangles disagree.
// The triangles facing away close it at both ends (see shadow_volume_render).

#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <graphics.h>
#include <render.h>
#include <matrix.h>
#include <shadow_volumes.h>
#include <normalmap.h>
#include <owl_packet.h>
#include <lz4_ps2.h>

void draw_vu1_with_colors(athena_object_data *obj, int pass_state);
extern MATRIX world_screen;

// ------------------------------------------------------------------ mesh --

static inline uint32_t sv_hash3(const float *p) {
	const uint32_t *w = (const uint32_t *)p;
	uint32_t h = 2166136261u;
	for (int i = 0; i < 3; i++)
		h = (h ^ w[i]) * 16777619u;
	return h;
}

static bool sv_build_mesh(ath_shadow_volume *v, athena_render_data *data) {
	const uint32_t n = data->index_count;

	// Triangles of the expanded vertex stream (list, or strip with restarts).
	uint32_t tri_max = data->tristrip ? (n > 2 ? n - 2 : 0) : n / 3;
	v->tri = malloc(tri_max * sizeof(*v->tri));
	uint32_t *weld = malloc(n * sizeof(uint32_t));
	uint32_t table_size = 1;
	while (table_size < n * 2)
		table_size <<= 1;
	int32_t *table = malloc(table_size * sizeof(int32_t));
	v->pos = alloc_vectors(n);
	if (!v->tri || !weld || !table || !v->pos) {
		free(weld); free(table);
		return false;
	}

	// Weld by position: the loaders split vertices on normals and UVs, which
	// would leave every crease an open edge.
	for (uint32_t s = 0; s < table_size; s++)
		table[s] = -1;
	v->pos_count = 0;
	for (uint32_t i = 0; i < n; i++) {
		const float *p = data->positions[i];
		uint32_t s = sv_hash3(p) & (table_size - 1);
		while (table[s] >= 0) {
			const float *q = v->pos[table[s]];
			if (q[0] == p[0] && q[1] == p[1] && q[2] == p[2])
				break;
			s = (s + 1) & (table_size - 1);
		}
		if (table[s] < 0) {
			table[s] = (int32_t)v->pos_count;
			v->pos[v->pos_count][0] = p[0];
			v->pos[v->pos_count][1] = p[1];
			v->pos[v->pos_count][2] = p[2];
			v->pos[v->pos_count][3] = 1.0f;
			v->pos_count++;
		}
		weld[i] = (uint32_t)table[s];
	}

	v->tri_count = 0;
	for (uint32_t i = 2; i < n; i += data->tristrip ? 1 : 3) {
		uint32_t a = weld[i - 2], b = weld[i - 1], c = weld[i];
		if (data->tristrip) {
			if (data->normals && data->normals[i][3] < 0.0f)
				continue;  // strip restart
			if (i & 1) { uint32_t t = a; a = b; b = t; }  // strips alternate winding
		}
		if (a == b || b == c || a == c)
			continue;
		v->tri[v->tri_count][0] = a;
		v->tri[v->tri_count][1] = b;
		v->tri[v->tri_count][2] = c;
		v->tri_count++;
	}
	free(weld);

	v->tri_normal = alloc_vectors(v->tri_count ? v->tri_count : 1);
	v->lit = malloc(v->tri_count ? v->tri_count : 1);
	for (uint32_t t = 0; t < v->tri_count; t++) {
		const float *a = v->pos[v->tri[t][0]], *b = v->pos[v->tri[t][1]], *c = v->pos[v->tri[t][2]];
		float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
		float e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
		v->tri_normal[t][0] = e1[1] * e2[2] - e1[2] * e2[1];
		v->tri_normal[t][1] = e1[2] * e2[0] - e1[0] * e2[2];
		v->tri_normal[t][2] = e1[0] * e2[1] - e1[1] * e2[0];
		v->tri_normal[t][3] = 0.0f;
	}

	// Edges: each triangle edge u->v either pairs with an earlier v->u, or
	// starts a new edge. An edge left without a second triangle is open.
	v->edges = malloc((v->tri_count * 3 + 1) * sizeof(*v->edges));
	if (!v->edges) {
		free(table);
		return false;
	}
	for (uint32_t s = 0; s < table_size; s++)
		table[s] = -1;
	v->edge_count = 0;
	for (uint32_t t = 0; t < v->tri_count; t++) {
		for (int k = 0; k < 3; k++) {
			uint32_t a = v->tri[t][k], b = v->tri[t][(k + 1) % 3];
			uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
			uint32_t s = (lo * 2654435761u ^ hi * 40503u) & (table_size - 1);
			int32_t found = -1;
			while (table[s] >= 0) {
				int32_t e = table[s];
				if (v->edges[e].a == b && v->edges[e].b == a && v->edges[e].t1 < 0) {
					found = e;
					break;
				}
				s = (s + 1) & (table_size - 1);
			}
			if (found >= 0) {
				v->edges[found].t1 = (int32_t)t;
				continue;
			}
			// Not paired: claim the next free slot of this chain.
			uint32_t e = v->edge_count++;
			v->edges[e].a = a;
			v->edges[e].b = b;
			v->edges[e].t0 = (int32_t)t;
			v->edges[e].t1 = -1;
			table[s] = (int32_t)e;
		}
	}
	free(table);
	return v->edge_count > 0;
}

static bool sv_build_slot(ath_shadow_volume *v, athena_render_data *data, athena_object_data *obj) {
	// A quad per edge, then the two caps: a triangle per triangle each.
	const uint32_t count = v->edge_count * 6 + v->tri_count * 6;

	memset(data, 0, sizeof(*data));
	data->index_count = count;
	data->positions = alloc_vectors(count);
	data->normals = alloc_vectors(count);
	data->colours = alloc_vectors(count);
	data->texcoords = NULL;
	data->materials = memalign(16, sizeof(ath_mat));
	data->material_indices = malloc(sizeof(material_index));
	if (!data->positions || !data->normals || !data->colours || !data->materials || !data->material_indices)
		return false;

	memset(data->positions, 0, count * sizeof(VECTOR));
	for (uint32_t i = 0; i < count; i++) {
		data->normals[i][0] = 0.0f; data->normals[i][1] = 1.0f; data->normals[i][2] = 0.0f; data->normals[i][3] = 1.0f;
		data->colours[i][0] = data->colours[i][1] = data->colours[i][2] = data->colours[i][3] = 0.0f;
		data->positions[i][3] = 1.0f;
	}

	// The colours program outputs (diffuse + vertex colour) * 128, truncated:
	// just over 1/128 gives exactly 1, the step each volume face adds.
	memset(data->materials, 0, sizeof(ath_mat));
	for (int k = 0; k < 4; k++)
		data->materials[0].diffuse[k] = 1.05f / 128.0f;
	data->materials[0].texture_id = -1;
	data->materials[0].bump_texture_id = -1;
	data->materials[0].decal_texture_id = -1;
	data->materials[0].ref_texture_id = -1;
	data->materials[0].normal_texture_id = -1;
	data->material_count = 1;
	data->material_indices[0].index = 0;
	data->material_indices[0].end = count - 1;
	data->material_index_count = 1;

	data->pipeline = PL_NO_LIGHTS;
	data->attributes.accurate_clipping = 1;  // volumes reach far past the screen
	data->attributes.face_culling = CULL_FACE_BACK;
	data->attributes.texture_mapping = 0;
	data->attributes.shade_model = SHADE_FLAT;
	data->tristrip = false;

	memset(obj, 0, sizeof(*obj));
	new_render_object(obj, data);
	obj->frustum_cull = false;
	return true;
}

static void sv_free_slot(athena_render_data *data) {
	athena_chain_cache *chain = &data->chain[PL_NO_LIGHTS][0];
	if (chain->has_queued)
		owl_wait_generation(chain->queued_gen);
	for (int p = 0; p < 3; p++) {
		free(data->chain[PL_NO_LIGHTS][p].buffer);
		free(data->chain[PL_NO_LIGHTS][p].chunk_offset);
		free(data->chain[PL_NO_LIGHTS][p].tex_giftag);
	}
	free(data->positions);
	free(data->normals);
	free(data->colours);
	free(data->materials);
	free(data->material_indices);
	free(data->compact_positions);
	free(data->compact_normals);
	free(data->compact_colors);
	free(data->compact_uvs);
	free(data->compact_group_base);
}

void shadow_volume_free(ath_shadow_volume *v) {
	if (!v)
		return;
	for (int s = 0; s < 2; s++)
		sv_free_slot(&v->slot[s]);
	free(v->pos);
	free(v->tri);
	free(v->tri_normal);
	free(v->lit);
	free(v->edges);
	free(v);
}

ath_shadow_volume *shadow_volume_create(athena_object_data *caster) {
	athena_render_data *data = caster ? caster->data : NULL;
	if (!data || data->frozen)
		return NULL;
	athena_mesh_ensure_uncompressed(data);
	if (!data->positions || data->index_count < 3)
		return NULL;

	ath_shadow_volume *v = calloc(1, sizeof(*v));
	if (!v)
		return NULL;
	v->caster = caster;
	v->extrusion = 20.0f;

	if (!sv_build_mesh(v, data) || !sv_build_slot(v, &v->slot[0], &v->obj[0]) || !sv_build_slot(v, &v->slot[1], &v->obj[1])) {
		shadow_volume_free(v);
		return NULL;
	}
	return v;
}

// ------------------------------------------------------------- GS state --

static struct {
	bool active;
	int rect[4];
	GSSURFACE fb, zv;
	uint64_t frame, zbuf, alpha, test, clamp;
	int abe;
} s_sv;

static GSSURFACE s_lut_identity = { 0 };
static GSSURFACE s_lut_zero = { 0 };
static GSSURFACE s_lut_shadow = { 0 };
static GSSURFACE s_lut_mask = { 0 };
static float s_lut_shadow_opacity = -1.0f;

static void sv_init_luts(void) {
	static uint32_t identity_mem[256] __attribute__((aligned(64)));
	static uint32_t zero_mem[256] __attribute__((aligned(64)));
	static uint32_t shadow_mem[256] __attribute__((aligned(64)));
	static uint32_t mask_mem[256] __attribute__((aligned(64)));
	if (s_lut_identity.Mem)
		return;

	uint32_t lin[256];
	for (int v = 0; v < 256; v++)
		lin[v] = v | (v << 8) | (v << 16) | ((uint32_t)v << 24);
	gs_clut_swizzle_csm1(identity_mem, lin);
	memset(zero_mem, 0, sizeof(zero_mem));

	s_lut_identity.Mem = identity_mem;
	s_lut_zero.Mem = zero_mem;
	s_lut_shadow.Mem = shadow_mem;
	// 0 (lit) -> alpha 0, any other count -> the alpha bit DATE tests.
	lin[0] = 0;
	for (int v = 1; v < 256; v++)
		lin[v] = 0x80u << 24;
	gs_clut_swizzle_csm1(mask_mem, lin);
	s_lut_mask.Mem = mask_mem;
}

static void sv_flush(void) {
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 1);
	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHA, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
}

static void sv_shuffle(GSSURFACE *dst, GSSURFACE *src, GSSURFACE *lut, uint32_t in, uint32_t out, uint64_t blend) {
	gs_channel_shuffle_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.in_channel = in;
	cfg.out_mask = out;
	cfg.x = s_sv.rect[0];
	cfg.y = s_sv.rect[1];
	cfg.width = s_sv.rect[2] - s_sv.rect[0];
	cfg.height = s_sv.rect[3] - s_sv.rect[1];
	cfg.alpha_blend = blend != 0;
	cfg.blend_equation = blend;
	cfg.z_test_method = DEPTH_ALWAYS;
	gs_channel_shuffle_ex(dst, src, lut, &cfg);
}

bool shadow_volumes_begin(const int *rect) {
	const int ctxt = gsGlobal->PrimContext;
	s_sv.active = false;
	s_sv.frame = get_register(GS_CACHE_FRAME + ctxt);
	s_sv.zbuf = get_register(GS_CACHE_ZBUF + ctxt);
	if (((s_sv.frame >> 24) & 0x3F) != GS_PSM_CT32 || ((s_sv.zbuf >> 24) & 0xF) != 1)
		return false;

	sv_init_luts();

	if (rect) {
		memcpy(s_sv.rect, rect, sizeof(s_sv.rect));
	} else {
		s_sv.rect[0] = 0; s_sv.rect[1] = 0;
		s_sv.rect[2] = gsGlobal->Width; s_sv.rect[3] = gsGlobal->Height;
	}
	if (s_sv.rect[2] <= s_sv.rect[0] || s_sv.rect[3] <= s_sv.rect[1])
		return false;

	memset(&s_sv.fb, 0, sizeof(s_sv.fb));
	s_sv.fb.Vram = (s_sv.frame & 0x1FF) * 8192;
	s_sv.fb.TBW = (s_sv.frame >> 16) & 0x3F;
	s_sv.fb.PSM = GS_PSM_CT32;
	s_sv.fb.Width = gsGlobal->Width;
	s_sv.fb.Height = gsGlobal->Height;
	s_sv.zv = s_sv.fb;
	s_sv.zv.Vram = (s_sv.zbuf & 0x1FF) * 8192;

	s_sv.alpha = get_screen_param(ALPHA_BLEND_EQUATION);
	s_sv.test = get_register(GS_CACHE_TEST + ctxt);
	s_sv.clamp = get_screen_param(COLOR_CLAMP_MODE);
	s_sv.abe = gsGlobal->PrimAlphaEnable;

	// Save red, then zero it: the stencil starts at 0 everywhere.
	sv_flush();
	sv_shuffle(&s_sv.zv, &s_sv.fb, &s_lut_identity, CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_ALPHA, 0);
	sv_shuffle(&s_sv.fb, &s_sv.fb, &s_lut_zero, CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_RED, 0);

	// Red only, modulo 256, depth tested but not written.
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	set_screen_param(COLOR_CLAMP_MODE, 0);
	set_screen_param(ALPHA_TEST_ENABLE, false);
	set_screen_param(DST_ALPHA_TEST_ENABLE, false);
	set_register(GS_CACHE_FRAME + ctxt, (s_sv.frame & 0xFFFFFFFFull) | (0xFFFFFF00ull << 32));
	set_register(GS_CACHE_ZBUF + ctxt, s_sv.zbuf | (1ull << 32));

	s_sv.active = true;
	return true;
}

void shadow_volumes_end(float opacity) {
	if (!s_sv.active)
		return;
	s_sv.active = false;

	const int ctxt = gsGlobal->PrimContext;
	sv_flush();
	set_register(GS_CACHE_FRAME + ctxt, s_sv.frame);
	set_register(GS_CACHE_ZBUF + ctxt, s_sv.zbuf);
	set_register(GS_CACHE_TEST + ctxt, s_sv.test);

	if (opacity < 0.0f) opacity = 0.0f;
	if (opacity > 1.0f) opacity = 1.0f;
	if (opacity != s_lut_shadow_opacity || !s_lut_shadow.Vram) {
		uint32_t lin[256];
		uint32_t shade = (uint32_t)(128.0f * (1.0f - opacity) + 0.5f);
		lin[0] = 0x80u << 24;
		for (int v = 1; v < 256; v++)
			lin[v] = shade << 24;
		gs_clut_swizzle_csm1(s_lut_shadow.Mem, lin);
		gs_upload_clut_inline(&s_lut_shadow, s_lut_shadow.Mem);
		s_lut_shadow_opacity = opacity;
	}

	// Stencil -> alpha; red back, times alpha; green and blue times alpha.
	set_screen_param(COLOR_CLAMP_MODE, 1);
	sv_shuffle(&s_sv.fb, &s_sv.fb, &s_lut_shadow, CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_ALPHA, 0);
	sv_shuffle(&s_sv.fb, &s_sv.zv, &s_lut_identity, CHANNEL_SHUFFLE_ALPHA, CHANNEL_SHUFFLE_RED,
	           ALPHA_EQUATION(SRC_RGB, ZERO_RGB, DST_ALPHA, ZERO_RGB, 0));
	sv_shuffle(&s_sv.fb, &s_sv.fb, &s_lut_identity, CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_GREEN | CHANNEL_SHUFFLE_BLUE,
	           ALPHA_EQUATION(DST_RGB, ZERO_RGB, DST_ALPHA, ZERO_RGB, 0));

	gsGlobal->PrimAlphaEnable = s_sv.abe;
	set_screen_param(ALPHA_BLEND_EQUATION, s_sv.alpha);
	set_screen_param(COLOR_CLAMP_MODE, s_sv.clamp);
}

void shadow_volumes_end_relight(athena_object_data *const *objs, int count) {
	if (!s_sv.active)
		return;
	s_sv.active = false;

	const int ctxt = gsGlobal->PrimContext;
	sv_flush();
	set_register(GS_CACHE_ZBUF + ctxt, s_sv.zbuf);
	set_register(GS_CACHE_FRAME + ctxt, s_sv.frame);
	set_register(GS_CACHE_TEST + ctxt, s_sv.test);
	if (!s_lut_mask.Vram)
		gs_upload_clut_inline(&s_lut_mask, s_lut_mask.Mem);

	// Stencil -> the alpha bit; red back.
	set_screen_param(COLOR_CLAMP_MODE, 1);
	sv_shuffle(&s_sv.fb, &s_sv.fb, &s_lut_mask, CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_ALPHA, 0);
	sv_shuffle(&s_sv.fb, &s_sv.zv, &s_lut_identity, CHANNEL_SHUFFLE_ALPHA, CHANNEL_SHUFFLE_RED, 0);

	// The objects unlit (texture times material colour), times the ambient
	// level, where the alpha bit is set and at their own depth. Alpha is kept.
	VECTOR light;
	float atten, ambient, diffuse;
	render_get_dot3_light(light, &atten, &ambient, &diffuse);
	int fix = (int)(ambient * 128.0f + 0.5f);
	fix = fix < 0 ? 0 : (fix > 128 ? 128 : fix);
	const uint64_t date = (1ull << 14) | (1ull << 15);  // DATE, DATM = pass where alpha bit 7 is 1
	set_register(GS_CACHE_TEST + ctxt, (s_sv.test & ~(1ull | date)) | date);
	set_register(GS_CACHE_FRAME + ctxt, s_sv.frame | (0xFF000000ull << 32));
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, ZERO_RGB, fix));
	for (int i = 0; i < count; i++) {
		if (objs[i] && objs[i]->data)
			draw_vu1_with_colors(objs[i], 0);
	}
	sv_flush();

	set_register(GS_CACHE_FRAME + ctxt, s_sv.frame);
	set_register(GS_CACHE_TEST + ctxt, s_sv.test);
	gsGlobal->PrimAlphaEnable = s_sv.abe;
	set_screen_param(ALPHA_BLEND_EQUATION, s_sv.alpha);
	set_screen_param(COLOR_CLAMP_MODE, s_sv.clamp);
}

// --------------------------------------------------------------- volumes --

// p pushed away from the light (w = 0: the direction towards it; w = 1: a point
// light's position), by the extrusion -- or less, to stay wmin in front of the
// eye. wrow: the clip matrix column that gives w, the view depth.
static inline void sv_extrude(const float *p, const VECTOR light, bool point, float extrusion,
                              const float wrow[4], float wmin, VECTOR out) {
	float dx = -light[0], dy = -light[1], dz = -light[2];
	if (point) {
		dx = p[0] - light[0]; dy = p[1] - light[1]; dz = p[2] - light[2];
		float d2 = dx*dx + dy*dy + dz*dz;
		float inv = d2 > 1e-12f ? 1.0f / sqrtf(d2) : 0.0f;
		dx *= inv; dy *= inv; dz *= inv;
	}
	float t = extrusion;
	float wd = dx*wrow[0] + dy*wrow[1] + dz*wrow[2];  // change of w per unit pushed
	if (wd < 0.0f)
		t = fmaxf(0.0f, fminf(t, (p[0]*wrow[0] + p[1]*wrow[1] + p[2]*wrow[2] + wrow[3] - wmin) / -wd));
	out[0] = p[0] + dx * t; out[1] = p[1] + dy * t; out[2] = p[2] + dz * t; out[3] = 1.0f;
}

void shadow_volume_render(ath_shadow_volume *v) {
	if (!s_sv.active || !v)
		return;

	VECTOR world_light, light;
	float atten, ambient, diffuse, k;
	if (!render_get_dot3_light(world_light, &atten, &ambient, &diffuse))
		return;

	// In the caster's object space (the same mapping the lit pipelines use):
	// the unit direction towards a directional light, or a point light's
	// position.
	render_dot3_light_to_object(v->caster->transform, world_light, atten, light, &k);
	const bool point = light[3] != 0.0f;
	if (!point && light[0]*light[0] + light[1]*light[1] + light[2]*light[2] < 1e-6f)
		return;

	athena_render_data *data = &v->slot[v->cur];
	athena_object_data *obj = &v->obj[v->cur];
	v->cur ^= 1;

	// This buffer's last draw was two frames ago; normally long done.
	athena_chain_cache *chain = &data->chain[PL_NO_LIGHTS][0];
	if (chain->has_queued && !owl_generation_read(chain->queued_gen))
		owl_wait_generation(chain->queued_gen);

	// Facing the light: towards it from any of the triangle's corners, for a
	// point light.
	for (uint32_t t = 0; t < v->tri_count; t++) {
		const float *n = v->tri_normal[t];
		float lx = light[0], ly = light[1], lz = light[2];
		if (point) {
			const float *c = v->pos[v->tri[t][0]];
			lx -= c[0]; ly -= c[1]; lz -= c[2];
		}
		v->lit[t] = (n[0]*lx + n[1]*ly + n[2]*lz) > 0.0f;
	}

	// Clip-space w is the view depth. A volume reaching back past the camera
	// would have to be clipped at the near plane, which loses its faces there
	// (and the shadow with them), so each extruded vertex stops a little in
	// front of it instead: the cut-off end sits where nothing visible is.
	MATRIX local_clip;
	matrix_functions->multiply(local_clip, v->caster->transform, world_screen);
	const float wrow[4] = { local_clip[3], local_clip[7], local_clip[11], local_clip[15] };
	const float wmin = render_get_near() * 1.5f;

	VECTOR *out = data->positions;
	for (uint32_t e = 0; e < v->edge_count; e++, out += 6) {
		const int32_t t0 = v->edges[e].t0, t1 = v->edges[e].t1;
		const bool l0 = v->lit[t0];
		const bool l1 = t1 >= 0 ? v->lit[t1] : false;
		const float *pa = v->pos[v->edges[e].a];

		if (l0 == l1) {
			// Not on the silhouette: a quad with no area.
			for (int k = 0; k < 6; k++) {
				out[k][0] = pa[0]; out[k][1] = pa[1]; out[k][2] = pa[2]; out[k][3] = 1.0f;
			}
			continue;
		}

		// Wound as in the lit triangle: a->b in t0, b->a in t1. Then the quad
		// u, v, v', u' faces out of the volume, like that triangle's front.
		const float *u = l0 ? pa : v->pos[v->edges[e].b];
		const float *w = l0 ? v->pos[v->edges[e].b] : pa;
		VECTOR q[4] = {
			{ u[0], u[1], u[2], 1.0f },
			{ 0 },
			{ w[0], w[1], w[2], 1.0f },
			{ 0 },
		};
		sv_extrude(w, light, point, v->extrusion, wrow, wmin, q[1]);
		sv_extrude(u, light, point, v->extrusion, wrow, wmin, q[3]);
		// u, w', w  and  u, u', w'
		copy_vector(out[0], q[0]); copy_vector(out[1], q[1]); copy_vector(out[2], q[2]);
		copy_vector(out[3], q[0]); copy_vector(out[4], q[3]); copy_vector(out[5], q[1]);
	}

	// The caps: the triangles facing away from the light, twice. Pushed along
	// with the sides (the same sv_extrude, so they meet exactly) and keeping
	// their winding -- away from the light, out of the volume -- they close the
	// far end, and the end cut off in front of the eye: the eye is never inside
	// a volume, and a short extrusion leaks no light. Where they are, reversed,
	// they close the near end: the volume starts at the caster's dark side
	// instead of taking its inside in, so that side, already unlit, is not
	// darkened a second time (it meets the cap at its own depth).
	for (uint32_t t = 0; t < v->tri_count; t++, out += 6) {
		if (v->lit[t]) {
			const float *pa = v->pos[v->tri[t][0]];
			for (int k = 0; k < 6; k++) {
				out[k][0] = pa[0]; out[k][1] = pa[1]; out[k][2] = pa[2]; out[k][3] = 1.0f;
			}
			continue;
		}
		for (int k = 0; k < 3; k++) {
			const float *p = v->pos[v->tri[t][k]];
			sv_extrude(p, light, point, v->extrusion, wrow, wmin, out[k]);
			out[5 - k][0] = p[0]; out[5 - k][1] = p[1]; out[5 - k][2] = p[2]; out[5 - k][3] = 1.0f;
		}
	}
	render_invalidate_compact_positions(data);

	memcpy(obj->transform, v->caster->transform, sizeof(MATRIX));

	const int ctxt = gsGlobal->PrimContext;
	(void)ctxt;
	// Front faces add one...
	data->attributes.face_culling = CULL_FACE_BACK;
	set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, 0x80));
	draw_vu1_with_colors(obj, 0);
	sv_flush();
	// ...back faces take it away.
	data->attributes.face_culling = CULL_FACE_FRONT;
	set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(DST_RGB, SRC_RGB, ALPHA_FIX, ZERO_RGB, 0x80));
	draw_vu1_with_colors(obj, 0);
	sv_flush();
}
