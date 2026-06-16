#ifndef TARFS_H
#define TARFS_H

#include <stdint.h>

void tarfs_init(void);
void tarfs_dir(void);
void tarfs_type(const char* filename);
uint64_t tarfs_get_file_size(const char* filename);
int tarfs_read_file(const char* filename, uint8_t* buffer);

#endif
