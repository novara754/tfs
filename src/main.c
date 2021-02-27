#include "tfs.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define UNUSED(x) ((void)(x))

static int source_fd;
static FILE *log;

void read_at(int fd, off_t off, void *buf, size_t len) {
  lseek(fd, off, 0);
  read(fd, buf, len);
}

void *tfs_fuse_init(struct fuse_conn_info *conn, struct fuse_config *conf) {
  UNUSED(conn);
  UNUSED(conf);

  tfs_instance *tfs = malloc(sizeof(*tfs));

  uint8_t buf[1];
  read_at(source_fd, 509, &buf, 1);
  tfs->reserved_blocks = buf[0];

  return tfs;
}

void tfs_fuse_destroy(void *private_data) {
  free(private_data);
  close(source_fd);
}

int tfs_fuse_getattr(const char *path, struct stat *st,
                     struct fuse_file_info *fi) {
  // if (strcmp(path, "/") == 0) {
  //   st->st_mode = __S_IFDIR | 0755;
  //   return 0;
  // }

  // if (strcmp(path, "/hello.txt") == 0) {
  //   st->st_mode = __S_IFREG | 0444;
  //   return 0;
  // }

  UNUSED(fi);

  fprintf(log, "now looking for path=%s\n", path);

  tfs_instance *instance = (tfs_instance *)fuse_get_context()->private_data;

  const char *next_slash = strchr(path + 1, '/');
  size_t disk_offset = tfs_get_data_block_offset(instance, 0);

  while (next_slash != NULL) {
    const char *segment = path + 1;
    size_t segment_len;
    if (next_slash == NULL) {
      segment_len = strlen(segment);
    } else {
      segment_len = next_slash - path - 1;
    }

    uint8_t buf[BLOCK_SIZE];
    read_at(source_fd, disk_offset, buf, BLOCK_SIZE);

    tfs_dir_ent *entry = tfs_find_dir_ent(segment, segment_len, buf);
    if (entry == NULL) {
      return -ENOTDIR;
    }

    if (next_slash != NULL && !entry->is_dir) {
      return -ENOENT;
    }

    disk_offset = tfs_get_data_block_offset(instance, entry->start_block);
    path = next_slash;
    next_slash = strchr(segment, '/');
  }

  // `disk_offset` is not set to the location of the parent directory
  // for the requested file.
  // `path` now points to the file's basename.
  const char *filename = path + 1;
  if (filename[0] == 0) {
    filename = ".";
  }

  fprintf(log, "filename=%s, disk_offset=%ld\n", filename, disk_offset);

  uint8_t buf[BLOCK_SIZE];
  read_at(source_fd, disk_offset, buf, BLOCK_SIZE);
  tfs_dir_ent *entry = tfs_find_dir_ent(filename, strlen(filename), buf);
  if (entry == NULL) {
    return -ENOENT;
  }

  fprintf(log, "entry->is_dir=%d\n", entry->is_dir);
  st->st_mode = entry->is_dir ? __S_IFDIR : __S_IFREG;

  fprintf(log, "now returning 0 from getattr\n");

  return 0;
}

int tfs_fuse_readdir(const char *path, void *dir_buf, fuse_fill_dir_t filler,
                     off_t off, struct fuse_file_info *fi,
                     enum fuse_readdir_flags flags) {
  UNUSED(off);
  UNUSED(fi);
  UNUSED(flags);

  tfs_instance *instance = (tfs_instance *)fuse_get_context()->private_data;

  if (strcmp(path, "/") == 0) {
    uint8_t buf[BLOCK_SIZE];
    read_at(source_fd, tfs_get_data_block_offset(instance, 0), buf, BLOCK_SIZE);

    filler(dir_buf, "..", NULL, 0, 0);

    for (size_t i = 0; i < BLOCK_SIZE; i += sizeof(tfs_dir_ent)) {
      tfs_dir_ent *entry = (tfs_dir_ent *)&buf[i];
      tfs_dir_ent_type type = tfs_get_dir_ent_type(entry);
      if (type == DIR_ENT_END) {
        break;
      }
      if (type == DIR_ENT_EMPTY) {
        continue;
      }

      entry->name[0] &= 0x7F;
      filler(dir_buf, (char *)entry->name, NULL, 0, 0);
    }

    return 0;
  }

  return -ENOENT;
}

int tfs_fuse_open(const char *path, struct fuse_file_info *fi) {
  UNUSED(path);
  UNUSED(fi);
  return -ENOENT;
}

int tfs_fuse_read(const char *path, char *buf, size_t size, off_t off,
                  struct fuse_file_info *fi) {
  UNUSED(path);
  UNUSED(buf);
  UNUSED(size);
  UNUSED(off);
  UNUSED(fi);
  return -ENOENT;
}

int tfs_fuse_write(const char *path, const char *buf, size_t size, off_t off,
                   struct fuse_file_info *fi) {
  UNUSED(path);
  UNUSED(buf);
  UNUSED(size);
  UNUSED(off);
  UNUSED(fi);
  return -ENOENT;
}

static const struct fuse_operations tfs_fuse_oper = {
    .init = tfs_fuse_init,
    .destroy = tfs_fuse_destroy,
    .getattr = tfs_fuse_getattr,
    .readdir = tfs_fuse_readdir,
    .open = tfs_fuse_open,
    .read = tfs_fuse_read,
    .write = tfs_fuse_write,
};

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "tfs: usage: %s <tfs> <mount-dir>\n", argv[0]);
    return 1;
  }


  char *source_path = realpath(argv[1], NULL);
  source_fd = open(source_path, O_RDWR);
  if (source_fd < 0) {
    perror("tfs: failed to open source file");
  }

  argv[1] = argv[0];
  argc--;

  log = fopen("./log.txt", "a");
  int ret = fuse_main(argc, argv + 1, &tfs_fuse_oper, NULL);

  return ret;
}
