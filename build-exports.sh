#!/bin/sh
mips64r5900el-ps2-elf-readelf -sW bin/tmp.elf | \
grep '\(GLOBAL\|WEAK\)' | \
grep -v '\(export_list\|UND\)' | \
awk '{
    if ($8 ~ /^[a-zA-Z_][a-zA-Z0-9_]*$/) 
        print $8
}' | \
sort -n | \
awk '{
    hidden_symbols["_GLOBAL__sub_I__ZN10__cxxabiv111__terminateEPFvvE"] = 1
    hidden_symbols["_GLOBAL__sub_I__ZN17__eh_globals_init7_S_initE"] = 1
    hidden_symbols["_GLOBAL__sub_I__ZN9__gnu_cxx9__freeresEv"] = 1
    hidden_symbols["_ZGVZN12_GLOBAL__N_116get_atomic_mutexEvE12atomic_mutex"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_110eh_globalsE"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_110fake_mutexE"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_114emergency_poolE"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_12mxE"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_14poolD1Ev"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_14poolD2Ev"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_19fake_condE"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_1L11static_condE"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_1L12static_mutexE"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_1L16init_static_condEv"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_1L4initEv"] = 1
    hidden_symbols["_ZZN12_GLOBAL__N_115get_static_condEvE4once"] = 1
    hidden_symbols["_ZZN12_GLOBAL__N_116get_atomic_mutexEvE12atomic_mutex"] = 1
    hidden_symbols["_ZZN12_GLOBAL__N_116get_static_mutexEvE4once"] = 1
    hidden_symbols["_GLOBAL__sub_I_SRand"] = 1
    hidden_symbols["_GLOBAL__sub_I__ZN13dxTriMeshDataC2Ev"] = 1
    hidden_symbols["_GLOBAL__sub_I__ZN6Opcode13MeshInterface11VertexCacheE"] = 1
    hidden_symbols["_GLOBAL__sub_I__ZSt7nothrow"] = 1
    hidden_symbols["_ZN12_GLOBAL__N_113__new_handlerE"] = 1

    if (!($0 in hidden_symbols)) {
        t[++count] = $0
    }
} 
END {
    printf("extern \"C\" {\n");
    for (i = 1; i <= count; i++) {
        if (t[i] == "main")
            printf("int %s();\n", t[i]);
        else
            printf("void %s();\n", t[i]);
    }
    printf("}\n");
    
    printf("struct export_list_t {\n");
    printf("    char * name;\n");
    printf("    void (*pointer)();\n");
    printf("} export_list[] = {\n");

    for (i = 1; i <= count; i++)
        printf("    { \"%s\", %s }, \n", t[i], t[i]);

    printf("    { 0, 0},\n};\n");
}' > src/exports.c