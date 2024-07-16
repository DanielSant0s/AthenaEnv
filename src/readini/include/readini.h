#ifndef READINI_H
#define READINI_H

#include <stdio.h>
#include <stdbool.h>

#define LINE_BUFSIZE 512

typedef struct {
    FILE* handle;
    char cur_line[LINE_BUFSIZE];
} IniReader;

bool readini_open(IniReader* ini, const char* fname);

bool readini_close(IniReader* ini);

bool readini_getline(IniReader* ini);

bool readini_bool(IniReader* ini, const char* key, bool* value_ptr);

bool readini_int(IniReader* ini, const char* key, int* value_ptr);

bool readini_float(IniReader* ini, const char* key, float* value_ptr);

bool readini_string(IniReader* ini, const char* key, char* value_ptr);

bool readini_comment(IniReader* ini, char* value_ptr);

bool readini_emptyline(IniReader* ini);

#endif
