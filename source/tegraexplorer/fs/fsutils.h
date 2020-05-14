#pragma once
#include "../common/types.h"
#include "../../utils/types.h"

char *fsutil_getnextloc(const char *current, const char *add);
char *fsutil_getprevloc(char *current);
bool fsutil_checkfile(char* path);
u64 fsutil_getfilesize(char *path);
int fsutil_getfolderentryamount(const char *path);
int extract_bis_file(char *path, char *outfolder);
char *fsutil_formatFileAttribs(char *path);