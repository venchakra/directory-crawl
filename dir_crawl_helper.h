#ifndef __DIRCRAWL_SOL_H__
#define __DIRCRAWL_SOL_H__

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/queue.h>

typedef enum file_type_e {
	FILE_TYPE_INVALID = 0,
	FILE_TYPE_REGULAR = 1,
	FILE_TYPE_DIR     = 2,
	FILE_TYPE_OTHER   = 3
} file_type_t;

extern file_type_t file_type(char *path);
extern char *next_file(char *dir_name, DIR *dir);

#endif // __DIRCRAWL_SOL_H__
