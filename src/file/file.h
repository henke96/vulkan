#pragma once

struct file__read {
    int result;
    long length;
    char *malloc_bytes;
};

struct file__read file__read(char *file_name);