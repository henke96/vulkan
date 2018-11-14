#include <stdio.h>
#include <malloc.h>
#include "file_util.h"

struct file_util__try_read file_util__try_read(char *file_name) {
	FILE *file;
	struct file_util__try_read result;
	file = fopen(file_name, "rb");
	if (!file) {
		result.result = -1;
		return result;
	}
	fseek(file, 0, SEEK_END);
	result.length = ftell(file);
	rewind(file);
	result.malloc_bytes = (char *) malloc(result.length*sizeof(char));
	fread(result.malloc_bytes, (size_t) result.length, 1, file);
	fclose(file);
	result.result = 0;
	return result;
}