/*
 * Playground, in C: the port of main.js. Vector2, Vector3 and Matrix4 from the
 * C API, and a direct kernel call (the script reaches it through System.nativeCall).
 */

#include <kernel.h>
#include <osd_config.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <athena/athena.h>
#include "athenaos/app.h"

static const AthenaOSHint HINTS[] = { { "D-pad", "Log input" } };

// A float as the script prints it: the shortest digits that read back the same.
static const char *num(float v, char *buf, size_t size) {
    for (int prec = 1; prec <= 9; prec++) {
        snprintf(buf, size, "%.*g", prec, v);
        if (strtof(buf, NULL) == v)
            break;
    }
    return buf;
}

static int playground_main(int argc, char **argv) {
    (void)argc; (void)argv;

    AthenaOSTerminal term;
    athenaos_terminal_init(&term, "Playground", "system", "Vector2, Vector3, Matrix4 and native calls", HINTS, 1);

    char buf[256];

    AthenaVec3 *v3 = athena_vec3_create(15.0f, 15.0f, 15.0f), *v4 = athena_vec3_create(30.0f, 30.0f, 30.0f);
    AthenaVec3 *sum3 = athena_vec3_add(v3, v4);
    athena_vec3_tostring(sum3, buf, sizeof(buf));
    athenaos_terminal_log(&term, "Vector3 %s", buf);
    char n[16];
    athenaos_terminal_log(&term, "Vector3 dist %s", num(athena_vec3_dist(v3, v4), n, sizeof(n)));

    AthenaVec2 *v1 = athena_vec2_create(15.0f, 15.0f), *v2 = athena_vec2_create(30.0f, 30.0f);
    athenaos_terminal_log(&term, "Vectors distance is %s", num(athena_vec2_dist(v1, v2), n, sizeof(n)));
    AthenaVec2 *(*const ops[])(const AthenaVec2 *, const AthenaVec2 *) = {
        athena_vec2_add, athena_vec2_sub, athena_vec2_mul, athena_vec2_div,
    };
    const char *const op_names[] = { "add", "sub", "mul", "div" };
    for (int i = 0; i < 4; i++) {
        AthenaVec2 *r = ops[i](v1, v2);
        athena_vec2_tostring(r, buf, sizeof(buf));
        athenaos_terminal_log(&term, "Vectors %s is %s", op_names[i], buf);
        athena_vec2_destroy(r);
    }

    AthenaMatrix4 *test_matrix = athena_matrix4_create();
    athena_matrix4_tostring(test_matrix, buf, sizeof(buf));
    athenaos_terminal_log(&term, "Matrix4 %s", buf);

    AthenaMatrix4 *m1 = athena_matrix4_create();
    if (athena_matrix4_equals(m1, test_matrix))
        athenaos_terminal_log(&term, "Matrix equal check for equal matrix values");

    for (int i = 0; i < ATHENA_MATRIX4_LENGTH; i++)
        athena_matrix4_set(test_matrix, i, athena_randomf(0.0f, 1.0f));
    athena_matrix4_tostring(test_matrix, buf, sizeof(buf));
    athenaos_terminal_log(&term, "Matrix4 %s", buf);

    if (!athena_matrix4_equals(m1, test_matrix))
        athenaos_terminal_log(&term, "Matrix equal check for different matrix values");

    // The kernel call the script makes through System.nativeCall.
    ConfigParam osd_config;
    memset(&osd_config, 0, sizeof(osd_config));
    GetOsdConfigParam(&osd_config);
    uint32_t raw;
    memcpy(&raw, &osd_config, sizeof(raw));
    athenaos_terminal_log(&term, "GetOsdConfigParam: %u", (unsigned)raw);

    AthenaPad *pad = athena_pad_open(0);
    for (;;) {
        athena_pad_update(pad);
        if (athenaos_home_requested(pad))
            break;
        if (athena_pad_just_pressed(pad, PAD_LEFT))
            athenaos_terminal_log(&term, "DPad Left button was just pressed");
        if (athena_pad_just_pressed(pad, PAD_RIGHT))
            athenaos_terminal_log(&term, "DPad Right button was just pressed");
        athenaos_terminal_draw(&term);
    }

    athena_matrix4_destroy(test_matrix);
    athena_matrix4_destroy(m1);
    athena_vec2_destroy(v1);
    athena_vec2_destroy(v2);
    athena_vec3_destroy(sum3);
    athena_vec3_destroy(v3);
    athena_vec3_destroy(v4);
    return 0;
}

ATHENAOS_APP(playground, playground_main)
