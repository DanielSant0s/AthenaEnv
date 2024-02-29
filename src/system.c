
#include <kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include "include/system.h"

/* Normalize a pathname by removing
  . and .. components, duplicated /, etc. */
char* __ps2_normalize_path(char *path_name)
{
	int i, j;
	int first, next;
	static char out[255];
	
	/* First copy the path into our temp buffer */
	strcpy(out, path_name);
        /* Then append "/" to make the rest easier */
	strcat(out,"/");

	/* Convert "//" to "/" */
	for(i=0; out[i+1]; i++) {
		if(out[i]=='/' && out[i+1]=='/') {
			for(j=i+1; out[j]; j++)
				out[j] = out[j+1];
			i--;
		}
	}

	/* Convert "/./" to "/" */
	for(i=0; out[i] && out[i+1] && out[i+2]; i++) {
		if(out[i]=='/' && out[i+1]=='.' && out[i+2]=='/') {
			for(j=i+1; out[j]; j++)
				out[j] = out[j+2];
			i--;
		}
	}

	/* Convert "/path/../" to "/" until we can't anymore.  Also
	 * convert leading "/../" to "/" */
	first = next = 0;
	while(1) {
		/* If a "../" follows, remove it and the parent */
		if(out[next+1] && out[next+1]=='.' && 
		   out[next+2] && out[next+2]=='.' &&
		   out[next+3] && out[next+3]=='/') {
			for(j=0; out[first+j+1]; j++)
				out[first+j+1] = out[next+j+4];
			first = next = 0;
			continue;
		}

		/* Find next slash */
		first = next;
		for(next=first+1; out[next] && out[next] != '/'; next++)
			continue;
		if(!out[next]) break;
	}

	/* Remove trailing "/" */
	for(i=1; out[i]; i++)
		continue;
	if(i >= 1 && out[i-1] == '/') 
		out[i-1] = 0;

	return (char*)out;
}
