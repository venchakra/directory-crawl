#include "dir_crawl_helper.h"

file_type_t file_type(char *path)
{
	struct stat stat_buf;

	int ret = stat(path, &stat_buf);

	if (ret == -1) {
		return FILE_TYPE_INVALID;
	}

	if (S_ISDIR(stat_buf.st_mode)) {
		return FILE_TYPE_DIR;
	}

	return FILE_TYPE_REGULAR;
}

char *next_file(char *dir_name, DIR *dir)
{
	struct dirent *dir_entry;

	do {
		dir_entry = readdir(dir);

		if (dir_entry == NULL) {
			return NULL;
		}

	} while (!strcmp(dir_entry->d_name, ".") ||
	         !strcmp(dir_entry->d_name, ".."));

	int file_path_len = strlen(dir_name) + 2 + strlen(dir_entry->d_name);

	char *file_path = (char *)malloc(file_path_len);
	if (file_path == NULL) {
		printf("Failed to allocate memory for file %s/%s\n",
		       dir_name, dir_entry->d_name);

		return NULL;
	}

	sprintf(file_path, "%s/%s", dir_name, dir_entry->d_name);

	return file_path;
}
