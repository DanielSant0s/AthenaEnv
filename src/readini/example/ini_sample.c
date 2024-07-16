#include <readini.h>
#include <stdio.h>

int _start() {
    IniReader ini;

    int test_int;
    bool test_bool;
    float test_float;
    char test_string[512];

    printf("Ini reading sample\n");

    if (readini_open(&ini, "ini_sample.ini")) {
        printf("Ini file successfully opened\n");
        while(readini_getline(&ini)) {
            if (readini_emptyline(&ini)) {
                printf("empty line\n");
                continue;
            } else if (readini_comment(&ini, test_string)) {
                printf("comment = %s\n", test_string);
            } else if (readini_bool(&ini, "false_bool_key", &test_bool)) {
                printf("false_bool_key = %s\n", test_bool? "true" : "false");

            } else if (readini_bool(&ini, "true_bool_key", &test_bool)) {
                printf("true_bool_key = %s\n", test_bool? "true" : "false");

            } else if (readini_int(&ini, "int_key", &test_int)) {
                printf("int_key = %d\n", test_int);

            } else if (readini_float(&ini, "float_key", &test_float)) {
                printf("float_key = %f\n", test_float);

            } else if (readini_string(&ini, "string_key", test_string)) {
                printf("string_key = %s\n", test_string);

            }
        }

        readini_close(&ini);
    }

    for(;;) {} // This does an infinite loop, because we'll use printfs on PCSX2 log system
    return 0;
}
