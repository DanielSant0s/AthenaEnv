#ifndef STRUTILS_H
#define STRUTILS_H

#ifdef __cplusplus
extern "C" {
#endif
/**
 * @brief  method returns true if it can extract needed info from path, otherwise false.
 * In case of true, it updates mountString, mountPoint and newCWD parameters with corresponding data if they are not NULL
 * It splits path by ":", and requires a minimum of 3 elements
 * @example if path = "hdd0:__common:pfs:/retroarch/" then: mountString = "pfs:", mountPoint = "hdd0:__common", newCWD = "pfs:/retroarch/"
 * @param path input parameter with full hdd path (`hdd0:__common:pfs:/retroarch/`)
 * @param mountString pointer to char* wich will contain pfs mountpoint (`pfs:`)
 * @param mountPoint returns the path of mounted partition (`hdd0:__common`)
 * @param newCWD returns the path to the file as pfs mount point string (`pfs:/retroarch/`)
 * @return true on success
*/
int getMountInfo(char *path, char *mountString, char *mountPoint, char *newCWD);

char* s_sprintf(const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif