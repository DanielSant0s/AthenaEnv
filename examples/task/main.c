/*
 * Task sample, in C: the port of main.js. Two EE threads beside the main loop:
 * one counts every 2 s, paced by a mutex the main loop releases once a frame;
 * the other is restarted every second and counts a pulse.
 */

#include <stdio.h>
#include <time.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = { { "cross", "Pause counters" }, { "square", "Drop objects" } };

static AthenaMutex *timer_mutex;
static AthenaTimer *timer;
static volatile int counter, pulse_counter;
static volatile bool counting = true;
static clock_t next_time;

static void pulse_thread(void *arg) {
    (void)arg;
    if (counting)
        pulse_counter++;
    athena_thread_exit();
}

static void loop_thread(void *arg) {
    (void)arg;
    for (;;) {
        athena_mutex_lock(timer_mutex);
        if (counting && athena_timer_get_time(timer) > next_time) {
            next_time = athena_timer_get_time(timer) + 2000000; // 2 seconds: the timer counts microseconds
            counter++;
        }
    }
}

static int task_main(int argc, char **argv) {
    (void)argc; (void)argv;

    athena_screen_set_param(DEPTH_TEST_ENABLE, false);

    AthenaOSApp ui;
    athenaos_app_init(&ui, "Task sample", "system", HINTS, 2);

    timer_mutex = athena_mutex_create();
    timer = athena_timer_create();
    counter = pulse_counter = 0;
    next_time = 0;
    counting = true;

    const int pulse = athena_thread_create("Thread: Pulse counter", pulse_thread, 16384, 16);
    const int loop = athena_thread_create("Thread: Loop counter", loop_thread, 16384, 16);

    athena_mutex_lock(timer_mutex);
    athena_thread_start(loop, NULL);

    AthenaPad *pad = athena_pad_open(0);
    AthenaTimer *interval = athena_timer_create();

    // Thread.list() of the script.
    AthenaThreadListing *threads = athena_system_list_threads();
    printf("[");
    for (int i = 0; threads && i < threads->count; i++)
        printf("%s{\"id\":%d,\"name\":\"%s\",\"status\":%d,\"stack\":%u}", i ? "," : "",
               threads->threads[i].id, threads->threads[i].title ? threads->threads[i].title : "",
               threads->threads[i].status, (unsigned)threads->threads[i].stack_size);
    printf("]\n");
    athena_system_thread_listing_free(threads);

    const Color ok = athenaos_rgba(70, 210, 120, 1.0f), warn = athenaos_rgba(255, 190, 60, 1.0f);
    const Color text = athenaos_rgba(255, 255, 255, 1.0f);

    for (;;) {
        athena_pad_update(pad);
        if (athena_pad_just_pressed(pad, PAD_CROSS))
            counting = !counting;
        // Square drops the script's objects for the garbage collector; nothing to drop here.

        // os.setInterval(..., 1000): the pulse thread, once a second.
        if (athena_timer_get_time(interval) >= CLOCKS_PER_SEC) {
            athena_timer_reset(interval);
            athena_thread_start(pulse, NULL);
        }

        athenaos_background(&ui);
        const float x = ui.content.x, y = ui.content.y, w = ui.content.w;
        athenaos_panel(&ui, x, y, w, 150);
        athenaos_heading(&ui, x + 20, y + 22, "Main thread");
        athenaos_text_v(&ui, x + 20, y + 46, "Hello from main thread!", 0.56f, text);
        athenaos_heading(&ui, x + 20, y + 78, "Aux threads");
        char line[64];
        snprintf(line, sizeof(line), "Loop counter (every 2 s): %d", counter);
        athenaos_text_v(&ui, x + 20, y + 102, line, 0.5f, text);
        snprintf(line, sizeof(line), "Pulse counter (os.setInterval): %d", pulse_counter);
        athenaos_text_v(&ui, x + 20, y + 124, line, 0.5f, text);
        athenaos_text_v_align(&ui, x + w - 20, y + 22, counting ? "RUNNING" : "PAUSED", 0.42f, counting ? ok : warn, ALIGN_RIGHT);
        athenaos_chrome(&ui);
        athena_screen_flip();

        athena_mutex_unlock(timer_mutex);

        if (athenaos_home_requested(pad))
            break;
    }

    athena_thread_kill(loop);
    athena_thread_kill(pulse);
    return 0;
}

ATHENAOS_APP(task, task_main)
