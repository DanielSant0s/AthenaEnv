#include <profiler.h>
#include <string.h>

#define PROF_STACK_DEPTH 16
#define PROF_CYCLES_PER_MS 294912.0f

static prof_section stack[PROF_STACK_DEPTH] = { PROF_SCRIPT };
static int depth = 0;       // index of the top of the stack
static int overflow = 0;    // pushes that did not fit, popped without effect

static uint32_t last_stamp = 0;
static uint32_t frame_start = 0;
static int started = 0;

static uint64_t acc[PROF_COUNT];
static uint64_t acc_frame = 0;
static uint32_t acc_frames = 0;

static prof_report report = { 0 };

static inline uint32_t prof_cycles(void) {
	uint32_t c;
	__asm__ __volatile__("mfc0 %0, $9" : "=r"(c));
	return c;
}

// Charges the cycles since the last event to the current top of the stack.
// uint32 subtraction is wrap-safe: Count wraps every ~14.5 s.
static inline uint32_t prof_charge(void) {
	uint32_t now = prof_cycles();
	acc[stack[depth]] += (uint32_t)(now - last_stamp);
	last_stamp = now;
	return now;
}

void prof_push(prof_section section) {
	prof_charge();
	if (depth + 1 >= PROF_STACK_DEPTH) {
		overflow++;
		return;
	}
	stack[++depth] = section;
}

void prof_pop(void) {
	prof_charge();
	if (overflow) {
		overflow--;
		return;
	}
	if (depth > 0)
		depth--;
}

void prof_frame_end(void) {
	uint32_t now = prof_charge();

	if (!started) {
		// First flip: nothing meaningful was measured before it.
		memset(acc, 0, sizeof(acc));
		frame_start = now;
		started = 1;
		return;
	}

	acc_frame += (uint32_t)(now - frame_start);
	frame_start = now;

	if (++acc_frames < PROF_WINDOW)
		return;

	float scale = 1.0f / (PROF_CYCLES_PER_MS * (float)acc_frames);
	report.frame_ms = (float)acc_frame * scale;
	for (int i = 0; i < PROF_COUNT; i++)
		report.section_ms[i] = (float)acc[i] * scale;
	report.frames = acc_frames;

	memset(acc, 0, sizeof(acc));
	acc_frame = 0;
	acc_frames = 0;
}

const prof_report *prof_get_report(void) {
	return &report;
}
