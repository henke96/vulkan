#pragma once

struct file_util__try_read {
	int result;
	long length;
	char *malloc_bytes;
}
file_util__try_read(char *file_name);