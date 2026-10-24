// DOT3 normal mapping -- see normalmap.h. The per-vertex half (tangent-space
// light, sphere-map coordinates) is draw_3D_dot3.vcl; this file prepares the
// normal maps and runs the GS side of both variants of the paper.
//
// No second 32-bit buffer fits in VRAM next to a double-buffered 640x448 frame
// and its Z, so the frame buffer itself is the paper's "dot product buffer",
// one object at a time, inside the object's screen rectangle:
//
//   1. red of the rectangle -> the unused top byte of Z24 (saved);
//   2. the geometry passes leave 128 + n.x*L.x/128 (and y, z) in R, G, B;
//   3. channel shuffles add G and B (and, 2-pass, 3x alpha) into R, modulo 256:
//      R = 128 + N.L;
//   4. the intensity palette turns R into the light level, written to alpha;
//   5. red comes back from Z;
//   6. the object is drawn once more, unlit, multiplied by that alpha.
//
// Only the object's own pixels keep anything from 2..4 (step 6 overwrites
// them); everywhere else in the rectangle only red and alpha were touched, and
// red is restored.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <graphics.h>
#include <matrix.h>
#include <render.h>
#include <normalmap.h>
#include <texture_manager.h>
#include <owl_packet.h>
#include <lz4_ps2.h>
#include <dbgprintf.h>

// render.c
extern MATRIX world_screen;
extern FIVECTOR screen_scale;
void draw_vu1_with_colors(athena_object_data *obj, int pass_state);

struct athena_dot3_map {
	GSSURFACE index;                 // PSMT8: one quantized normal per texel
	GSSURFACE pal[DOT3_PASS_COUNT];  // the palette each pass reads it through (SPHERE unused)
	uint32_t *pal_mem;               // DOT3_PASS_COUNT * 256 entries, CSM1 order
};

// ---------------------------------------------------------------- palettes --

static inline int dot3_clamp(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

// The same normals seen by every pass, n in [-127, 127] per component:
//   POSITIVE  rgb = max(n, 0)
//   NEGATIVE  rgb = max(-n, 0), a = (sum of 128 - n) / 3 -- the correction term
//             (128 + max(-n,0) - max(n,0) per channel), a third of it in alpha
//   X, Y, Z   rgb = 128, a = 128 + n.x (n.y, n.z)
static void dot3_build_palettes(athena_dot3_map *map, const int8_t (*n)[3], int count) {
	uint32_t lin[256];

	for (int p = 0; p < DOT3_PASS_COUNT; p++) {
		for (int i = 0; i < 256; i++) {
			int x = 0, y = 0, z = 127;
			if (i < count) {
				x = n[i][0]; y = n[i][1]; z = n[i][2];
			}

			int r = 0, g = 0, b = 0, a = 0x80;
			switch (p) {
				case DOT3_PASS_POSITIVE:
					r = x > 0 ? x : 0; g = y > 0 ? y : 0; b = z > 0 ? z : 0;
					break;
				case DOT3_PASS_NEGATIVE:
					r = x < 0 ? -x : 0; g = y < 0 ? -y : 0; b = z < 0 ? -z : 0;
					a = dot3_clamp((384 - x - y - z + 1) / 3, 0, 255);
					break;
				case DOT3_PASS_X: r = g = b = 128; a = 128 + x; break;
				case DOT3_PASS_Y: r = g = b = 128; a = 128 + y; break;
				case DOT3_PASS_Z: r = g = b = 128; a = 128 + z; break;
				default: break;
			}
			lin[i] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | ((uint32_t)a << 24);
		}

		gs_clut_swizzle_csm1(&map->pal_mem[p * 256], lin);

		GSSURFACE *pal = &map->pal[p];
		memset(pal, 0, sizeof(*pal));
		pal->Mem = &map->pal_mem[p * 256];
	}
}

static athena_dot3_map *dot3_map_alloc(int width, int height) {
	athena_dot3_map *map = calloc(1, sizeof(*map));
	if (!map)
		return NULL;

	map->pal_mem = memalign(64, DOT3_PASS_COUNT * 256 * sizeof(uint32_t));
	map->index.Mem = memalign(128, width * height);
	if (!map->pal_mem || !map->index.Mem) {
		free(map->pal_mem);
		free(map->index.Mem);
		free(map);
		return NULL;
	}

	map->index.Width = width;
	map->index.Height = height;
	map->index.PSM = GS_PSM_T8;
	map->index.ClutPSM = GS_PSM_CT32;
	map->index.ClutStorageMode = GS_CLUT_STORAGE_CSM1;
	map->index.Filter = GS_FILTER_LINEAR;
	athena_calculate_tbw(&map->index);
	return map;
}

static void dot3_map_free(athena_dot3_map *map) {
	if (!map)
		return;

	texture_manager_free(&map->index);
	for (int p = 0; p < DOT3_PASS_COUNT; p++) {
		if (map->pal[p].Vram)
			texture_manager_free(&map->pal[p]);
	}
	free(map->index.Mem);
	free(map->pal_mem);
	free(map);
}

// ------------------------------------------------------------ quantization --

// Texel as 0x00BBGGRR, whatever the texture's format.
static bool dot3_read_texels(GSSURFACE *tex, uint32_t *out) {
	athena_texture_ensure_uncompressed(tex);
	if (!tex->Mem)
		return false;

	const int count = tex->Width * tex->Height;
	switch (tex->PSM) {
		case GS_PSM_CT32: {
			const uint32_t *src = (const uint32_t *)tex->Mem;
			for (int i = 0; i < count; i++)
				out[i] = src[i] & 0x00FFFFFF;
			return true;
		}
		case GS_PSM_CT24: {
			const uint8_t *src = (const uint8_t *)tex->Mem;
			for (int i = 0; i < count; i++)
				out[i] = src[i*3] | (src[i*3+1] << 8) | (src[i*3+2] << 16);
			return true;
		}
		case GS_PSM_CT16:
		case GS_PSM_CT16S: {
			const uint16_t *src = (const uint16_t *)tex->Mem;
			for (int i = 0; i < count; i++) {
				uint32_t v = src[i];
				out[i] = ((v & 31) << 3) | (((v >> 5) & 31) << 11) | (((v >> 10) & 31) << 19);
			}
			return true;
		}
		case GS_PSM_T8: {
			if (!tex->Clut)
				return false;
			const uint8_t *src = (const uint8_t *)tex->Mem;
			for (int i = 0; i < count; i++) {
				// The palette is kept in CSM1 order (gs_clut_swizzle_csm1).
				int idx = src[i];
				int rem = idx & 0x18;
				if (rem == 0x08 || rem == 0x10)
					idx ^= 0x18;
				out[i] = tex->Clut[idx] & 0x00FFFFFF;
			}
			return true;
		}
		default:
			return false;
	}
}

typedef struct {
	uint32_t start, count;
} dot3_box;

// Sorts [start, start + count) of cols/ids by one 8-bit channel (counting sort).
static void dot3_sort_box(uint32_t *cols, uint32_t *ids, uint32_t *tmp_c, uint32_t *tmp_i,
                          uint32_t start, uint32_t count, int shift) {
	uint32_t hist[257];
	memset(hist, 0, sizeof(hist));
	for (uint32_t k = start; k < start + count; k++)
		hist[((cols[k] >> shift) & 0xFF) + 1]++;
	for (int v = 0; v < 256; v++)
		hist[v + 1] += hist[v];
	for (uint32_t k = start; k < start + count; k++) {
		uint32_t at = hist[(cols[k] >> shift) & 0xFF]++;
		tmp_c[at] = cols[k];
		tmp_i[at] = ids[k];
	}
	memcpy(&cols[start], tmp_c, count * sizeof(uint32_t));
	memcpy(&ids[start], tmp_i, count * sizeof(uint32_t));
}

// Median cut: up to 256 boxes of similar normals, each texel indexed to its box.
static athena_dot3_map *dot3_quantize(GSSURFACE *tex) {
	const uint32_t n = tex->Width * tex->Height;
	uint32_t *cols = malloc(n * sizeof(uint32_t));
	uint32_t *ids = malloc(n * sizeof(uint32_t));
	uint32_t *tmp_c = malloc(n * sizeof(uint32_t));
	uint32_t *tmp_i = malloc(n * sizeof(uint32_t));
	athena_dot3_map *map = NULL;

	if (!cols || !ids || !tmp_c || !tmp_i || !dot3_read_texels(tex, cols))
		goto out;

	for (uint32_t i = 0; i < n; i++)
		ids[i] = i;

	dot3_box boxes[256];
	int box_count = 1;
	boxes[0].start = 0;
	boxes[0].count = n;

	while (box_count < 256) {
		// Split the box with the widest channel range.
		int best = -1, best_shift = 0, best_range = 0;
		for (int b = 0; b < box_count; b++) {
			if (boxes[b].count < 2)
				continue;
			for (int shift = 0; shift <= 16; shift += 8) {
				int lo = 255, hi = 0;
				for (uint32_t k = boxes[b].start; k < boxes[b].start + boxes[b].count; k++) {
					int v = (cols[k] >> shift) & 0xFF;
					if (v < lo) lo = v;
					if (v > hi) hi = v;
				}
				if (hi - lo > best_range) {
					best_range = hi - lo;
					best = b;
					best_shift = shift;
				}
			}
		}
		if (best < 0)
			break;  // every box holds a single colour

		dot3_box *box = &boxes[best];
		dot3_sort_box(cols, ids, tmp_c, tmp_i, box->start, box->count, best_shift);

		// Split at the median, moved off a run of equal values so that both
		// halves are non-empty and no colour straddles the cut.
		uint32_t mid = box->start + box->count / 2;
		uint32_t end = box->start + box->count;
		uint32_t mv = (cols[mid] >> best_shift) & 0xFF;
		uint32_t cut = mid;
		while (cut > box->start && ((cols[cut - 1] >> best_shift) & 0xFF) == mv)
			cut--;
		if (cut == box->start) {
			cut = mid;
			while (cut < end && ((cols[cut] >> best_shift) & 0xFF) == mv)
				cut++;
		}

		boxes[box_count].start = cut;
		boxes[box_count].count = end - cut;
		box->count = cut - box->start;
		box_count++;
	}

	map = dot3_map_alloc(tex->Width, tex->Height);
	if (!map)
		goto out;

	int8_t normals[256][3];
	uint8_t *index = (uint8_t *)map->index.Mem;
	for (int b = 0; b < box_count; b++) {
		float sx = 0.0f, sy = 0.0f, sz = 0.0f;
		for (uint32_t k = boxes[b].start; k < boxes[b].start + boxes[b].count; k++) {
			sx += (float)(cols[k] & 0xFF) * (2.0f / 255.0f) - 1.0f;
			sy += (float)((cols[k] >> 8) & 0xFF) * (2.0f / 255.0f) - 1.0f;
			sz += (float)((cols[k] >> 16) & 0xFF) * (2.0f / 255.0f) - 1.0f;
			index[ids[k]] = (uint8_t)b;
		}
		float len = sqrtf(sx*sx + sy*sy + sz*sz);
		float inv = len > 1e-6f ? 127.0f / len : 0.0f;
		normals[b][0] = (int8_t)lrintf(sx * inv);
		normals[b][1] = (int8_t)lrintf(sy * inv);
		normals[b][2] = (int8_t)(len > 1e-6f ? lrintf(sz * inv) : 127);
	}

	dot3_build_palettes(map, (const int8_t (*)[3])normals, box_count);
	map->index.Clut = map->pal_mem; // for the texture manager; passes override it
	SyncDCache(map->index.Mem, (uint8_t *)map->index.Mem + n);

out:
	free(cols);
	free(ids);
	free(tmp_c);
	free(tmp_i);
	return map;
}

// --------------------------------------------------------- shared surfaces --

// Materials without a normal map: one flat normal, i.e. per-pixel N.L from
// the interpolated light alone.
static athena_dot3_map *s_flat_map = NULL;

// The 4-pass renormalization table: sphere-map texel -> 128 - 127 * unit vector
// (the paper's Appendix 1).
static GSSURFACE s_sphere = { 0 };

// Shuffle palettes: identity, x3 (the 2-pass alpha term) and the two
// intensity palettes (the dot products of the two variants are scaled
// differently).
static GSSURFACE s_lut_identity = { 0 };
static GSSURFACE s_lut_triple = { 0 };
static GSSURFACE s_lut_ilp[2] = { { 0 }, { 0 } };
static float s_ilp_ambient[2] = { -1.0f, -1.0f };
static float s_ilp_diffuse[2] = { -1.0f, -1.0f };

static int dot3_nearest(float x) {
	return x < 0.0f ? -(int)(-x + 0.5f) : (int)(x + 0.5f);
}

static bool dot3_init_shared(void) {
	if (s_sphere.Mem)
		return s_flat_map != NULL;

	const int w = 64, h = 64;
	uint32_t *mem = memalign(128, w * h * sizeof(uint32_t));
	if (!mem)
		return false;

	for (int y = 0; y < h; y++) {
		for (int x = 0; x < w; x++) {
			const float s = (x + 0.5f) / (float)w;
			const float t = (y + 0.5f) / (float)h;
			const float s2 = 2.0f * s - 1.0f;
			const float t2 = 2.0f * t - 1.0f;
			// s = lx/m + 0.5, t = ly/m + 0.5, m = 2 sqrt(2 lz + 2), |l| = 1 gives lz = 1 - 2 (s2^2 + t2^2)
			float lz = 1.0f - (s2*s2 + t2*t2) * 2.0f;
			int r = 128, g = 128, b = 128;
			if (lz >= -1.0f) {
				const float m = 2.0f * sqrtf(2.0f * lz + 2.0f);
				float lx = (s - 0.5f) * m;
				float ly = (t - 0.5f) * m;
				const float len = sqrtf(lx*lx + ly*ly + lz*lz);
				lx /= len; ly /= len; lz /= len;
				r = 128 - dot3_nearest(lx * 127.0f);
				g = 128 - dot3_nearest(ly * 127.0f);
				b = 128 - dot3_nearest(lz * 127.0f);
			}
			mem[y * w + x] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | (0x80u << 24);
		}
	}
	SyncDCache(mem, mem + w * h);

	s_sphere.Width = w;
	s_sphere.Height = h;
	s_sphere.PSM = GS_PSM_CT32;
	s_sphere.Filter = GS_FILTER_LINEAR;
	s_sphere.Mem = mem;
	athena_calculate_tbw(&s_sphere);

	s_flat_map = dot3_map_alloc(8, 8);
	if (!s_flat_map)
		return false;
	static const int8_t flat[1][3] = { { 0, 0, 127 } };
	memset(s_flat_map->index.Mem, 0, 64);
	SyncDCache(s_flat_map->index.Mem, (uint8_t *)s_flat_map->index.Mem + 64);
	dot3_build_palettes(s_flat_map, flat, 1);
	s_flat_map->index.Clut = s_flat_map->pal_mem;

	uint32_t lin[256];
	static uint32_t identity_mem[256] __attribute__((aligned(64)));
	static uint32_t triple_mem[256] __attribute__((aligned(64)));
	static uint32_t ilp_mem[2][256] __attribute__((aligned(64)));

	for (int v = 0; v < 256; v++)
		lin[v] = v | (v << 8) | (v << 16) | ((uint32_t)v << 24);
	gs_clut_swizzle_csm1(identity_mem, lin);
	for (int v = 0; v < 256; v++) {
		uint32_t t = (3 * v) & 0xFF;
		lin[v] = t | (t << 8) | (t << 16) | (t << 24);
	}
	gs_clut_swizzle_csm1(triple_mem, lin);

	s_lut_identity.Mem = identity_mem;
	s_lut_triple.Mem = triple_mem;
	s_lut_ilp[0].Mem = ilp_mem[0];
	s_lut_ilp[1].Mem = ilp_mem[1];
	return true;
}

// Intensity palette: R = 128 + dot, dot = D * cos(angle) with D the variant's
// full-scale product (127 * 122 / 128 for 2-pass, 127 * 127 / 128 for 4-pass).
// Alpha 128 = 1.0, the same ceiling the Gouraud pipelines clamp light to.
static void dot3_update_ilp(int variant, float ambient, float diffuse) {
	if (s_ilp_ambient[variant] == ambient && s_ilp_diffuse[variant] == diffuse && s_lut_ilp[variant].Vram)
		return;

	const float full = variant == 0 ? (127.0f * 122.0f / 128.0f) : (127.0f * 127.0f / 128.0f);
	uint32_t lin[256];
	for (int v = 0; v < 256; v++) {
		float c = (float)(v - 128) / full;
		if (c < 0.0f) c = 0.0f;
		if (c > 1.0f) c = 1.0f;
		float level = ambient + diffuse * c;
		if (level < 0.0f) level = 0.0f;
		if (level > 1.0f) level = 1.0f;
		uint32_t a = (uint32_t)(level * 128.0f + 0.5f);
		lin[v] = a << 24;
	}
	gs_clut_swizzle_csm1(s_lut_ilp[variant].Mem, lin);
	gs_upload_clut_inline(&s_lut_ilp[variant], s_lut_ilp[variant].Mem);

	s_ilp_ambient[variant] = ambient;
	s_ilp_diffuse[variant] = diffuse;
}

// ------------------------------------------------------------- mesh state --

static athena_dot3_state *dot3_state(athena_render_data *data) {
	if (!data->dot3)
		data->dot3 = calloc(1, sizeof(athena_dot3_state));
	return data->dot3;
}

static void dot3_free_chains(athena_dot3_state *st) {
	for (int p = 0; p < DOT3_PASS_COUNT; p++) {
		athena_chain_cache *chain = &st->chains[p];
		if (chain->has_queued)
			owl_wait_generation(chain->queued_gen);
		free(chain->buffer);
		free(chain->chunk_offset);
		free(chain->tex_giftag);
		memset(chain, 0, sizeof(*chain));
	}
}

void normalmap_release(athena_render_data *data, bool keep_maps) {
	athena_dot3_state *st = data->dot3;
	if (!st)
		return;

	dot3_free_chains(st);
	free(st->tangents);
	free(st->compact_tangents);
	st->tangents = NULL;
	st->compact_tangents = NULL;
	st->compact_capacity = 0;
	st->cooked_version = 0;
	st->unsupported = false;

	if (keep_maps)
		return;

	for (int i = 0; i < st->map_count; i++)
		dot3_map_free(st->maps[i]);
	free(st->maps);
	free(st);
	data->dot3 = NULL;
}

// Exact copies of a vertex (same position, normal and UV) share one tangent:
// the loaders expand indexed meshes, so a smooth surface reaches here as one
// copy per triangle, and per-triangle tangents would facet the lighting.
typedef struct {
	float key[8];
} dot3_vertex_key;

static inline void dot3_make_key(const athena_render_data *data, uint32_t v, dot3_vertex_key *k) {
	k->key[0] = data->positions[v][0]; k->key[1] = data->positions[v][1]; k->key[2] = data->positions[v][2];
	k->key[3] = data->normals[v][0];   k->key[4] = data->normals[v][1];   k->key[5] = data->normals[v][2];
	k->key[6] = data->texcoords[v][0]; k->key[7] = data->texcoords[v][1];
}

static inline uint32_t dot3_hash_key(const dot3_vertex_key *k) {
	uint32_t h = 2166136261u;
	const uint32_t *w = (const uint32_t *)k->key;
	for (int i = 0; i < 8; i++)
		h = (h ^ w[i]) * 16777619u;
	return h;
}

bool normalmap_build_tangents(athena_render_data *data, athena_dot3_state *st) {
	if (data->frozen)
		return false;
	athena_mesh_ensure_uncompressed(data);
	if (!data->positions || !data->normals || !data->texcoords || data->index_count < 3)
		return false;

	const uint32_t n = data->index_count;
	VECTOR *tan = alloc_vectors(n);
	VECTOR *bit = alloc_vectors(n);
	uint32_t *rep = malloc(n * sizeof(uint32_t));
	uint32_t table_size = 1;
	while (table_size < n * 2)
		table_size <<= 1;
	int32_t *table = malloc(table_size * sizeof(int32_t));
	if (!tan || !bit || !rep || !table) {
		free(tan); free(bit); free(rep); free(table);
		return false;
	}
	memset(tan, 0, n * sizeof(VECTOR));
	memset(bit, 0, n * sizeof(VECTOR));

	// Per triangle: T along +U, B along -V. Image rows (and V) run downwards,
	// while a normal map's green points up the image.
	for (uint32_t i = 2; i < n; i += data->tristrip ? 1 : 3) {
		uint32_t a = i - 2, b = i - 1, c = i;
		if (data->tristrip && data->normals[c][3] < 0.0f)
			continue;  // strip restart: this vertex completes no triangle

		const float *p0 = data->positions[a], *p1 = data->positions[b], *p2 = data->positions[c];
		const float *t0 = data->texcoords[a], *t1 = data->texcoords[b], *t2 = data->texcoords[c];
		float e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
		float e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };
		float du1 = t1[0] - t0[0], dv1 = t1[1] - t0[1];
		float du2 = t2[0] - t0[0], dv2 = t2[1] - t0[1];
		float r = du1 * dv2 - du2 * dv1;
		if (fabsf(r) < 1e-12f)
			continue;
		float f = 1.0f / r;
		for (int k = 0; k < 3; k++) {
			float tk = (e1[k] * dv2 - e2[k] * dv1) * f;
			float bk = -(e2[k] * du1 - e1[k] * du2) * f;
			tan[a][k] += tk; tan[b][k] += tk; tan[c][k] += tk;
			bit[a][k] += bk; bit[b][k] += bk; bit[c][k] += bk;
		}
	}

	// Weld exact copies: each vertex points at the first copy of itself.
	for (uint32_t s = 0; s < table_size; s++)
		table[s] = -1;
	for (uint32_t v = 0; v < n; v++) {
		dot3_vertex_key kv, ko;
		dot3_make_key(data, v, &kv);
		uint32_t s = dot3_hash_key(&kv) & (table_size - 1);
		while (table[s] >= 0) {
			dot3_make_key(data, (uint32_t)table[s], &ko);
			if (memcmp(&kv, &ko, sizeof(kv)) == 0)
				break;
			s = (s + 1) & (table_size - 1);
		}
		if (table[s] < 0)
			table[s] = (int32_t)v;
		rep[v] = (uint32_t)table[s];
	}
	for (uint32_t v = 0; v < n; v++) {
		if (rep[v] == v)
			continue;
		for (int k = 0; k < 3; k++) {
			tan[rep[v]][k] += tan[v][k];
			bit[rep[v]][k] += bit[v][k];
		}
	}

	// Gram-Schmidt against each vertex's own normal; w = handedness.
	VECTOR *out = alloc_vectors(n);
	if (!out) {
		free(tan); free(bit); free(rep); free(table);
		return false;
	}
	for (uint32_t v = 0; v < n; v++) {
		const float *T = tan[rep[v]], *B = bit[rep[v]];
		float N[3] = { data->normals[v][0], data->normals[v][1], data->normals[v][2] };
		float nl = sqrtf(N[0]*N[0] + N[1]*N[1] + N[2]*N[2]);
		if (nl > 1e-8f) { N[0] /= nl; N[1] /= nl; N[2] /= nl; }

		float d = N[0]*T[0] + N[1]*T[1] + N[2]*T[2];
		float t[3] = { T[0] - N[0]*d, T[1] - N[1]*d, T[2] - N[2]*d };
		float tl = sqrtf(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
		if (tl < 1e-8f) {
			// No usable UV gradient: any direction across the normal.
			float ax[3] = { fabsf(N[0]) < 0.9f ? 1.0f : 0.0f, fabsf(N[0]) < 0.9f ? 0.0f : 1.0f, 0.0f };
			float da = N[0]*ax[0] + N[1]*ax[1] + N[2]*ax[2];
			t[0] = ax[0] - N[0]*da; t[1] = ax[1] - N[1]*da; t[2] = ax[2] - N[2]*da;
			tl = sqrtf(t[0]*t[0] + t[1]*t[1] + t[2]*t[2]);
		}
		out[v][0] = t[0] / tl;
		out[v][1] = t[1] / tl;
		out[v][2] = t[2] / tl;

		float cx = N[1]*out[v][2] - N[2]*out[v][1];
		float cy = N[2]*out[v][0] - N[0]*out[v][2];
		float cz = N[0]*out[v][1] - N[1]*out[v][0];
		out[v][3] = (cx*B[0] + cy*B[1] + cz*B[2]) < 0.0f ? -1.0f : 1.0f;
	}

	free(tan); free(bit); free(rep); free(table);
	free(st->tangents);
	st->tangents = out;
	st->cooked_version = 0;
	return true;
}

static athena_dot3_map *dot3_map_for(athena_render_data *data, athena_dot3_state *st, const ath_mat *mat) {
	int id = mat->normal_texture_id;
	if (id < 0 || id >= data->texture_count || !data->textures[id])
		return s_flat_map;

	if (st->map_count < data->texture_count) {
		athena_dot3_map **maps = realloc(st->maps, data->texture_count * sizeof(*maps));
		if (!maps)
			return s_flat_map;
		memset(&maps[st->map_count], 0, (data->texture_count - st->map_count) * sizeof(*maps));
		st->maps = maps;
		st->map_count = data->texture_count;
	}
	if (!st->maps[id]) {
		st->maps[id] = dot3_quantize(data->textures[id]);
		if (!st->maps[id]) {
			dbgprintf("normalmap: texture %d is not a usable normal map\n", id);
			return s_flat_map;
		}
	}
	return st->maps[id];
}

typedef struct {
	athena_dot3_state *st;
	eDot3Pass pass;
} dot3_bind_ctx;

static void dot3_bind(void *vctx, athena_render_data *data, const ath_mat *mat,
                      GSSURFACE **tex, GSSURFACE **clut) {
	dot3_bind_ctx *ctx = (dot3_bind_ctx *)vctx;
	if (ctx->pass == DOT3_PASS_SPHERE) {
		*tex = &s_sphere;
		*clut = NULL;
		return;
	}
	athena_dot3_map *map = dot3_map_for(data, ctx->st, mat);
	*tex = &map->index;
	*clut = &map->pal[ctx->pass];
}

// ---------------------------------------------------------------- drawing --

// Screen rectangle of the object's box, { x0, y0, x1, y1 }, x1/y1 exclusive.
static int dot3_screen_rect(athena_object_data *obj, int out[4]) {
	const int screen_w = gsGlobal->Width;
	const int screen_h = gsGlobal->Height;
	const VECTOR *box = obj->data->bounding_box;
	MATRIX local_screen;
	matrix_functions->multiply(local_screen, obj->transform, world_screen);

	float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
	for (int i = 0; i < 8; i++) {
		VECTOR clip;
		matrix_functions->apply(clip, local_screen, (float *)box[i]);
		if (clip[3] < 0.05f) {
			out[0] = 0; out[1] = 0; out[2] = screen_w; out[3] = screen_h;
			return 1;
		}
		float inv_w = 1.0f / clip[3];
		float sx = clip[0] * inv_w * screen_scale.x + screen_scale.x;
		float sy = clip[1] * inv_w * screen_scale.y + screen_scale.y;
		if (sx < x0) x0 = sx;
		if (sx > x1) x1 = sx;
		if (sy < y0) y0 = sy;
		if (sy > y1) y1 = sy;
	}

	int ix0 = (int)x0 - 1, iy0 = (int)y0 - 1;
	int ix1 = (int)x1 + 2, iy1 = (int)y1 + 2;
	if (ix0 < 0) ix0 = 0;
	if (iy0 < 0) iy0 = 0;
	if (ix1 > screen_w) ix1 = screen_w;
	if (iy1 > screen_h) iy1 = screen_h;
	if (ix0 >= ix1 || iy0 >= iy1)
		return 0;

	out[0] = ix0; out[1] = iy0; out[2] = ix1; out[3] = iy1;
	return 1;
}

// Keeps a GS register write behind the geometry already queued: the passes'
// primitives go out on PATH1 from VU1, register writes on PATH2.
static void dot3_flush(void) {
	owl_packet *packet = owl_query_packet(CHANNEL_VIF1, 1);
	owl_add_cnt_tag(packet, 0, owl_vif_code_double(VIF_CODE(0, 0, VIF_FLUSHA, 0), VIF_CODE(0, 0, VIF_NOP, 0)));
}

static void dot3_set_frame_mask(uint64_t frame, uint32_t mask) {
	set_register(GS_CACHE_FRAME + gsGlobal->PrimContext, (frame & 0xFFFFFFFFull) | ((uint64_t)mask << 32));
}

static void dot3_shuffle(GSSURFACE *dst, GSSURFACE *src, GSSURFACE *lut, uint32_t in, uint32_t out,
                         uint64_t blend, const int rect[4]) {
	gs_channel_shuffle_config cfg;
	memset(&cfg, 0, sizeof(cfg));
	cfg.in_channel = in;
	cfg.out_mask = out;
	cfg.x = rect[0];
	cfg.y = rect[1];
	cfg.width = rect[2] - rect[0];
	cfg.height = rect[3] - rect[1];
	cfg.alpha_blend = blend != 0;
	cfg.blend_equation = blend;
	cfg.z_test_method = DEPTH_ALWAYS;
	gs_channel_shuffle_ex(dst, src, lut, &cfg);
}

static void dot3_geometry(athena_object_data *obj, athena_dot3_state *st, eDot3Pass pass,
                          int tfx, float kl, float kf, float kc, float ks) {
	dot3_bind_ctx ctx = { st, pass };
	render_dot3_pass desc;
	desc.pass = pass;
	desc.tfx = tfx;
	desc.mode[0] = kl; desc.mode[1] = kf; desc.mode[2] = kc; desc.mode[3] = ks;
	desc.bind = dot3_bind;
	desc.ctx = &ctx;
	render_draw_dot3_pass(obj, &desc);
	dot3_flush();
}

// Uploads the palettes this object's maps use in the given passes.
static void dot3_upload_palettes(athena_render_data *data, athena_dot3_state *st, eDot3Pass first, eDot3Pass last) {
	athena_dot3_map *done[8];
	int done_count = 0;

	for (int i = 0; i < data->material_index_count; i++) {
		const ath_mat *mat = &data->materials[data->material_indices[i].index];
		athena_dot3_map *map = dot3_map_for(data, st, mat);
		bool seen = false;
		for (int k = 0; k < done_count; k++)
			seen |= (done[k] == map);
		if (seen)
			continue;
		for (int p = first; p <= last; p++)
			gs_upload_clut_inline(&map->pal[p], map->pal[p].Mem);
		if (done_count < 8)
			done[done_count++] = map;
	}
}

bool normalmap_render_object(athena_object_data *obj) {
	athena_render_data *data = obj->data;
	const int mode = data->attributes.normal_mapping;

	if ((mode != DOT3_2PASS && mode != DOT3_4PASS) || !data->attributes.texture_mapping || data->skeleton)
		return false;

	// The frame buffer is the scratch buffer, and the top byte of a Z24
	// buffer the only spare one: anything else draws the usual way.
	const int ctxt = gsGlobal->PrimContext;
	const uint64_t frame = get_register(GS_CACHE_FRAME + ctxt);
	const uint64_t zbuf = get_register(GS_CACHE_ZBUF + ctxt);
	if (((frame >> 24) & 0x3F) != GS_PSM_CT32 || ((zbuf >> 24) & 0xF) != 1)
		return false;

	athena_dot3_state *st = dot3_state(data);
	if (!st || st->unsupported || !dot3_init_shared())
		return false;
	if (!st->tangents && !normalmap_build_tangents(data, st)) {
		st->unsupported = true;
		return false;
	}

	int rect[4];
	if (!dot3_screen_rect(obj, rect))
		return true;

	VECTOR light;
	float atten, ambient, diffuse;
	render_get_dot3_light(light, &atten, &ambient, &diffuse);
	dot3_update_ilp(mode == DOT3_2PASS ? 0 : 1, ambient, diffuse);

	// Views of the current draw and depth buffers, the latter as plain CT32
	// so that its top byte can be read and written.
	GSSURFACE fb, zv;
	memset(&fb, 0, sizeof(fb));
	fb.Vram = (frame & 0x1FF) * 8192;
	fb.TBW = (frame >> 16) & 0x3F;
	fb.PSM = GS_PSM_CT32;
	fb.Width = gsGlobal->Width;
	fb.Height = gsGlobal->Height;
	zv = fb;
	zv.Vram = (zbuf & 0x1FF) * 8192;

	const uint64_t saved_alpha = get_screen_param(ALPHA_BLEND_EQUATION);
	const uint64_t saved_clamp = get_screen_param(COLOR_CLAMP_MODE);
	const uint64_t saved_test = get_register(GS_CACHE_TEST + ctxt);
	const int saved_abe = gsGlobal->PrimAlphaEnable;
	const uint32_t user_mask = (uint32_t)(frame >> 32);

	const uint64_t blend_copy = ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, ZERO_RGB, 0x80);
	const uint64_t blend_add  = ALPHA_EQUATION(SRC_RGB, ZERO_RGB, ALPHA_FIX, DST_RGB, 0x80);

	// 1. Save red.
	dot3_flush();
	dot3_shuffle(&zv, &fb, &s_lut_identity, CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_ALPHA, 0, rect);

	// 2. The signed products, modulo 256.
	gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
	set_screen_param(COLOR_CLAMP_MODE, 0);
	set_screen_param(ALPHA_TEST_ENABLE, false);
	set_screen_param(DST_ALPHA_TEST_ENABLE, false);

	if (mode == DOT3_2PASS) {
		dot3_upload_palettes(data, st, DOT3_PASS_POSITIVE, DOT3_PASS_NEGATIVE);
		dot3_set_frame_mask(frame, 0);
		// max(n, 0) * L
		set_screen_param(ALPHA_BLEND_EQUATION, blend_copy);
		dot3_geometry(obj, st, DOT3_PASS_POSITIVE, COLOR_MODULATE, 1.0f, 0.0f, 0.0f, 0.0f);
		// - max(-n, 0) * L; alpha takes the correction term
		set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(DST_RGB, SRC_RGB, ALPHA_FIX, ZERO_RGB, 0x80));
		dot3_geometry(obj, st, DOT3_PASS_NEGATIVE, COLOR_MODULATE, 1.0f, 0.0f, 0.0f, 0.0f);
	} else {
		dot3_upload_palettes(data, st, DOT3_PASS_X, DOT3_PASS_Z);
		dot3_set_frame_mask(frame, 0xFF000000);
		// 128 - 127 * fade * normalize(L)
		set_screen_param(ALPHA_BLEND_EQUATION, blend_copy);
		dot3_geometry(obj, st, DOT3_PASS_SPHERE, COLOR_HIGHLIGHT, 0.0f, 1.0f, 0.0f, 1.0f);
		// (128 - fb) * (128 + n) / 128 + fb = 128 + l * n / 128, one channel each
		set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, DST_RGB, SRC_ALPHA, DST_RGB, 0));
		dot3_set_frame_mask(frame, 0xFFFFFF00);
		dot3_geometry(obj, st, DOT3_PASS_X, COLOR_MODULATE, 0.0f, 0.0f, 1.0f, 0.0f);
		dot3_set_frame_mask(frame, 0xFFFF00FF);
		dot3_geometry(obj, st, DOT3_PASS_Y, COLOR_MODULATE, 0.0f, 0.0f, 1.0f, 0.0f);
		dot3_set_frame_mask(frame, 0xFF00FFFF);
		dot3_geometry(obj, st, DOT3_PASS_Z, COLOR_MODULATE, 0.0f, 0.0f, 1.0f, 0.0f);
	}
	dot3_set_frame_mask(frame, user_mask);

	// 3. R = R + G + B (+ 3A): 128 + N.L
	dot3_shuffle(&fb, &fb, &s_lut_identity, CHANNEL_SHUFFLE_GREEN, CHANNEL_SHUFFLE_RED, blend_add, rect);
	dot3_shuffle(&fb, &fb, &s_lut_identity, CHANNEL_SHUFFLE_BLUE, CHANNEL_SHUFFLE_RED, blend_add, rect);
	if (mode == DOT3_2PASS)
		dot3_shuffle(&fb, &fb, &s_lut_triple, CHANNEL_SHUFFLE_ALPHA, CHANNEL_SHUFFLE_RED, blend_add, rect);

	// 4. Light level into alpha. 5. Red back.
	dot3_shuffle(&fb, &fb, &s_lut_ilp[mode == DOT3_2PASS ? 0 : 1], CHANNEL_SHUFFLE_RED, CHANNEL_SHUFFLE_ALPHA, 0, rect);
	dot3_shuffle(&fb, &zv, &s_lut_identity, CHANNEL_SHUFFLE_ALPHA, CHANNEL_SHUFFLE_RED, 0, rect);

	// 6. The object unlit, times the light level.
	set_screen_param(COLOR_CLAMP_MODE, 1);
	set_register(GS_CACHE_TEST + ctxt, saved_test);
	set_screen_param(ALPHA_BLEND_EQUATION, ALPHA_EQUATION(SRC_RGB, ZERO_RGB, DST_ALPHA, ZERO_RGB, 0));
	dot3_set_frame_mask(frame, user_mask | 0xFF000000);
	draw_vu1_with_colors(obj, 0);
	dot3_flush();

	dot3_set_frame_mask(frame, user_mask);
	gsGlobal->PrimAlphaEnable = saved_abe;
	set_screen_param(ALPHA_BLEND_EQUATION, saved_alpha);
	set_screen_param(COLOR_CLAMP_MODE, saved_clamp);
	return true;
}
