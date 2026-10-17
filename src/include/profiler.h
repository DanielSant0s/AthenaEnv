#ifndef ATHENA_PROFILER_H
#define ATHENA_PROFILER_H

#include <stdint.h>

// Frame profiler on the EE cycle counter (COP0 Count, 294.912 MHz).
//
// Sections nest: prof_push/prof_pop keep a small stack and every cycle is
// charged to whatever is on top, so time is never counted twice -- a GPU wait
// that happens inside render_object() shows up as GPU wait, not render.
// PROF_SCRIPT sits at the bottom and absorbs everything not inside another
// section: the JS frame logic, 2D drawing, the interpreter itself.
//
// prof_frame_end() is called by the flip. Reports are averages over
// PROF_WINDOW frames, so they are stable enough to read off a HUD.
typedef enum {
	PROF_SCRIPT = 0,  // JS + anything not covered below
	PROF_RENDER,      // render_object(): culling, packet building (EE side)
	PROF_ANIM,        // skeletal animation + bone palette on the EE
	PROF_PHYSICS,     // physics world step
	PROF_GPU_WAIT,    // EE blocked on DMA / GS FINISH (GPU is the bottleneck)
	PROF_VSYNC,       // EE blocked waiting for vertical sync
	PROF_COUNT
} prof_section;

#define PROF_WINDOW 30

typedef struct {
	float frame_ms;              // average EE time between two flips
	float section_ms[PROF_COUNT];
	uint32_t frames;             // frames in the last window (0 = no report yet)
} prof_report;

void prof_push(prof_section section);
void prof_pop(void);
void prof_frame_end(void);
const prof_report *prof_get_report(void);

#endif
