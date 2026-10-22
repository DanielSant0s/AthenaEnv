/*
 * newlib's malloc lock, replaced.
 *
 * The default __malloc_lock/__malloc_unlock go through ps2sdk's retarget
 * locks: a recursive mutex built on kernel semaphores, so every malloc and
 * free costs GetThreadId + WaitSema + SignalSema syscalls. Profiling the
 * script runtimes showed those syscalls taking over half the time of
 * allocation-heavy code.
 *
 * The EE has a single core and threads only switch on interrupts or
 * syscalls, so disabling interrupts makes the heap update atomic for a few
 * hundred cycles at most. DIntr/EIntr are plain di/ei instructions, not
 * syscalls. The lock is recursive: only the outermost unlock restores the
 * interrupt state found by the outermost lock.
 */

#include <kernel.h>
#include <reent.h>

static int lock_depth = 0;
static int lock_interrupts_were_on = 0;

void __malloc_lock(struct _reent *r)
{
    int was_on = DIntr();
    (void)r;
    if (lock_depth++ == 0)
        lock_interrupts_were_on = was_on;
}

void __malloc_unlock(struct _reent *r)
{
    (void)r;
    if (--lock_depth == 0 && lock_interrupts_were_on)
        EIntr();
}
