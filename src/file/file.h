#pragma once

struct file__try_read {
	int result;
	long length;
	char *malloc_bytes;
}
file__try_read(char *file_name);