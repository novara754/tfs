#ifndef TFS_H
#define TFS_H

#include <stddef.h>
#include <stdint.h>

#define BLOCK_SIZE 1024
#define BLOCK_USAGE_BITMAP_SIZE (2 * BLOCK_SIZE)

typedef enum {
  DIR_ENT_USED,
  DIR_ENT_EMPTY,
  DIR_ENT_END,
} tfs_dir_ent_type;

typedef struct {
  uint8_t reserved_blocks;
} tfs_instance;

typedef struct __attribute__((packed)) {
  union {
    uint8_t name[10];
    struct {
      uint8_t is_dir : 1;
      uint8_t padding_1 : 7;
      uint8_t padding_2[9];
    };
  };
  uint16_t start_block;
  uint16_t num_blocks;
  uint16_t size;
} tfs_dir_ent;

/**
 * Calculate the byte offset for a given block index.
 */
size_t tfs_get_block_offset(size_t idx);

/**
 * Calculate the byte offset for a given data block index
 * while respecting the number of configured reserved blocks
 * at the start.
 */
size_t tfs_get_data_block_offset(const tfs_instance *instance, size_t idx);

/**
 * Get the state of the given directory entry.
 *
 * DIR_ENT_USED    - A used directory entry.
 * DIR_ENT_EMPTY   - An empty directory entry.
 * DIR_ENT_END     - An empty directory entry marking the end of the directory.
 */
tfs_dir_ent_type tfs_get_dir_ent_type(const tfs_dir_ent *entry);

/**
 * Find a file or directory entry in a directory block by its name.
 *
 * Return NULL if entry not found.
 */
tfs_dir_ent *tfs_find_dir_ent(const char *name, size_t name_len,
                              void *entries);

#endif // TFS_H
