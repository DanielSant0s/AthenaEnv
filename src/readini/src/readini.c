#include <readini.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool readini_open(IniReader* ini, const char* fname) {
    ini->handle = fopen(fname, "r");
    return ini->handle;
}

bool readini_close(IniReader* ini) {
    return fclose(ini->handle);
}

bool readini_getline(IniReader* ini) {
    char* v1 = ini->cur_line;

    if( !fgets(ini->cur_line, LINE_BUFSIZE, ini->handle) ) {
        if(!feof(ini->handle)) {
            ini->cur_line[0] = '\0';
            return true;
        }
        return false;
    }

    char first_char = ini->cur_line[0];
    if ( ini->cur_line[0] )
    {
      char* v4 = ini->cur_line;
      do
      {
        if ( *v4 < ' ' || *v4 == ',' )
          *v4 = ' ';
      }
      while ( *++v4 );
      first_char = ini->cur_line[0];
    }

    for ( ; first_char <= ' '; first_char = *++v1 )
    {
      if ( !first_char )
        break;
    }

    strcpy(ini->cur_line, v1);

    return true;
}

bool readini_comment(IniReader* ini, char* value_ptr) {
    if (ini->cur_line[0] == '#') {
        strcpy(value_ptr, ini->cur_line+1);
        return true;
    } else if (ini->cur_line[0] == '/' && ini->cur_line[1] == '/') {
        strcpy(value_ptr, ini->cur_line+2);
        return true;
    }

    return false;
}

bool readini_emptyline(IniReader* ini) {
    if(ini->cur_line[0]) {
        return false;
    }
    return true;
}

bool readini_bool(IniReader* ini, const char* key, bool* value_ptr) {
    int ret = true;

    char tmp_str[512];
    strcpy(tmp_str, ini->cur_line);

    char* key_str = strtok(tmp_str, " ,\t=");
    char* value_str = strtok(NULL, " ,\t=");

    ret = strcasecmp(key_str, key);
    if ( !ret )
    {   
        if (!strcasecmp("false", value_str)) {
            *value_ptr = false;
            ret = false;
        } else if (!strcasecmp("true", value_str)) {
            *value_ptr = true;
            ret = false;
        } else {
            ret = true;
        }
        
    }

    return !ret;
}

bool readini_int(IniReader* ini, const char* key, int* value_ptr) {
    int ret = true;

    char tmp_str[512];
    strcpy(tmp_str, ini->cur_line);

    char* key_str = strtok(tmp_str, " ,\t=");
    char* value_str = strtok(NULL, " ,\t=");

    ret = strcasecmp(key_str, key);
    if ( !ret )
    {
        *value_ptr = atoi(value_str);
    }

    return !ret;
}

bool readini_float(IniReader* ini, const char* key, float* value_ptr) {
    int ret = true;

    char tmp_str[512];
    strcpy(tmp_str, ini->cur_line);

    char* key_str = strtok(tmp_str, " ,\t=");
    char* value_str = strtok(NULL, " ,\t=");

    ret = strcasecmp(key_str, key);
    if ( !ret )
    {
        *value_ptr = strtof(value_str, NULL);;
    }

    return !ret;
}

bool readini_string(IniReader* ini, const char* key, char* value_ptr) {
    int ret = true;

    char tmp_str[512];
    char tmp_real_str[512];
    strcpy(tmp_str, ini->cur_line);

    char* key_str = strtok(tmp_str, " ,\t=");
    char* value_str = strtok(NULL, " ,\t=");

    strcpy(tmp_real_str, ini->cur_line+(value_str-tmp_str));

    ret = strcasecmp(key_str, key);
    if ( !ret )
    {
        if(*tmp_real_str == '"' || *tmp_real_str == '\'') {
            strcpy(value_ptr, tmp_real_str+1);
            value_ptr[strlen(value_ptr)-1] = '\0';
        } else {
            strcpy(value_ptr, tmp_real_str);
        }

    }

    return !ret;
}
