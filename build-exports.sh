#!/bin/sh
mips64r5900el-ps2-elf-readelf -sW bin/tmp.elf | \
grep '\(GLOBAL\|WEAK\)' | \
grep -v '\(export_list\|UND\|HIDDEN\)' | \
awk '{
    if ($8 ~ /^[a-zA-Z_][a-zA-Z0-9_]*$/) 
        print $8
}' | \
sort -n | \
awk '{
    t[NR] = $0
} 
END {
    for (i = 1; i < NR; i++)
        printf("void %s();\n", t[i]);
    
    printf("struct export_list_t {\n");
    printf("    char * name;\n");
    printf("    void * pointer;\n");
    printf("} export_list[] = {\n");

    for (i = 1; i < NR; i++)
        printf("    { \"%s\", %s }, \n", t[i], t[i]);

    printf("    { 0, 0},\n};\n");
}' > src/exports.c