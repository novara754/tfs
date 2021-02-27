#include "tfs.h"
#include <string.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))

size_t tfs_get_block_offset(size_t idx) {
  return idx * BLOCK_SIZE;
}

size_t tfs_get_data_block_offset(const tfs_instance *instance, size_t idx) {
  return (instance->reserved_blocks + 1) * BLOCK_SIZE +
         BLOCK_USAGE_BITMAP_SIZE + idx * BLOCK_SIZE;
}

tfs_dir_ent_type tfs_get_dir_ent_type(const tfs_dir_ent *entry) {
  uint8_t *data = (uint8_t *)entry;

  int empty = 1;
  int marks_end = 1;

  for (size_t i = 0; i < sizeof(tfs_dir_ent); i++) {
    if (data[i] != 0x00) {
      marks_end = 0;
    }
    if (data[i] != 0x7F) {
      empty = 0;
    }
  }

  if (empty) {
    return DIR_ENT_EMPTY;
  }
  if (marks_end) {
    return DIR_ENT_END;
  }
  return DIR_ENT_USED;
}

tfs_dir_ent *tfs_find_dir_ent(const char *name, size_t name_len,
                              void *buf) {
  tfs_dir_ent *entries = (tfs_dir_ent *)buf;

  for (size_t i = 0; i < (BLOCK_SIZE / sizeof(tfs_dir_ent)); i++) {
    tfs_dir_ent_type type = tfs_get_dir_ent_type(&entries[i]);
    if (type == DIR_ENT_END) {
      return NULL;
    }

    if (type == DIR_ENT_EMPTY) {
      continue;
    }

    if (name[0] == (entries[i].name[0] & 0x7F) &&
        strncmp(name + 1, (char *)entries[i].name + 1, MIN(name_len, 10)) == 0) {
      return &entries[i];
    }
  }

  return NULL;
}
