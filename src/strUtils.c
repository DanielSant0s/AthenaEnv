
#include "include/strUtils.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

int getMountInfo(char *path, char *mountString, char *mountPoint, char *newCWD)
{
    int expected_items = 4;
    int i = 0;
    char *items[expected_items];
    char** tokens = str_split(path, ':');

    if (!tokens)
        return 0;

    for (i = 0; *(tokens + i); i++) {
        if (i < expected_items) {
            items[i] = *(tokens + i);
        } else {
            free(*(tokens + i));    
        }
    }

    if (i < 3 )
        return 0;

    if (mountPoint != NULL)
        sprintf(mountPoint, "%s:%s", items[0], items[1]);

    if (mountString != NULL)
        sprintf(mountString, "%s:", items[2]);

    if (newCWD != NULL)
        sprintf(newCWD, "%s:%s", items[2], i > 3 ? items[3] : "");

    free(items[0]);
    free(items[1]);
    free(items[2]);

    if (i > 3)
        free(items[3]);

    return 1;
}