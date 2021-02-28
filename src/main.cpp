#include "tfs.h"
#include "util.h"
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iostream>

#define UNUSED(x) ((void)(x))

static int source_fd;

auto get_instance() -> tfs::tfs_instance {
  return *reinterpret_cast<tfs::tfs_instance *>(
      fuse_get_context()->private_data);
}

auto read_at(int fd, off_t off, void *buf, size_t len) {
  lseek(fd, off, 0);
  read(fd, buf, len);
}

void *tfs_fuse_init(struct fuse_conn_info *conn, struct fuse_config *conf) {
  UNUSED(conn);
  conf->kernel_cache = 1;
  return new tfs::tfs_instance{[](off_t off, void *buf, size_t len) {
    lseek(source_fd, off, 0);
    read(source_fd, buf, len);
  }};
}

void tfs_fuse_destroy(void *private_data) {
  auto tfs = reinterpret_cast<tfs::tfs_instance *>(private_data);
  delete tfs;
  close(source_fd);
}

int tfs_fuse_getattr(const char *path, struct stat *st,
                     struct fuse_file_info *fi) {
  UNUSED(fi);

  auto instance = get_instance();
  auto entry = instance.get_dir_ent_for_path(path);
  if (!entry.has_value()) {
    return -ENOENT;
  }

  st->st_nlink = 1;
  if (entry->is_dir()) {
    st->st_mode = __S_IFDIR;
  } else {
    st->st_mode = __S_IFREG;
    st->st_size = entry->total_size();
  }

  return 0;
}

int tfs_fuse_readdir(const char *path, void *dir_buf, fuse_fill_dir_t filler,
                     off_t off, struct fuse_file_info *fi,
                     enum fuse_readdir_flags flags) {
  UNUSED(off);
  UNUSED(fi);
  UNUSED(flags);

  auto instance = get_instance();

  auto entries = instance.read_dir(path);
  if (!entries.has_value()) {
    return -ENOTDIR;
  }

  // Normally the root directory does not have a parent entry,
  // but for the FUSE it's needed so it's added manually.
  if (path == std::string("/")) {
    filler(dir_buf, "..", NULL, 0, static_cast<fuse_fill_dir_flags>(0));
  }

  for (const auto &entry : entries.value()) {
    filler(dir_buf, entry.clean_name().c_str(), NULL, 0,
           static_cast<fuse_fill_dir_flags>(0));
  }

  return 0;
}

int tfs_fuse_open(const char *path, struct fuse_file_info *fi) {
  auto instance = get_instance();
  auto entry = instance.get_dir_ent_for_path(path);
  if (!entry.has_value()) {
    return -ENOENT;
  }
  if ((fi->flags & O_ACCMODE) != O_RDONLY) {
    return -EACCES;
  }
  return 0;
}

int tfs_fuse_read(const char *path, char *buf, size_t size, off_t off,
                  struct fuse_file_info *fi) {
  UNUSED(fi);

  auto instance = get_instance();
  auto contents = instance.read_file(path);

  if (!contents.has_value()) {
    return -ENOENT;
  }

  if (static_cast<size_t>(off) >= contents->size()) {
    return 0;
  }

  if (off + size > contents->size()) {
    size = contents->size() - off;
  }

  std::copy_n(contents->begin(), size, buf);

  return size;
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

int main(int argc, char **argv) {
  if (argc < 3) {
    fprintf(stderr, "tfs: usage: %s <tfs> <mount-dir> [options]\n", argv[0]);
    return 1;
  }

  char *source_path = realpath(argv[1], NULL);
  source_fd = open(source_path, O_RDWR);
  if (source_fd < 0) {
    perror("tfs: failed to open source file");
  }

  argv[1] = argv[0];
  argc--;

  static struct fuse_operations tfs_fuse_oper;
  tfs_fuse_oper.init = tfs_fuse_init;
  tfs_fuse_oper.destroy = tfs_fuse_destroy;
  tfs_fuse_oper.getattr = tfs_fuse_getattr;
  tfs_fuse_oper.readdir = tfs_fuse_readdir;
  tfs_fuse_oper.open = tfs_fuse_open;
  tfs_fuse_oper.read = tfs_fuse_read;
  tfs_fuse_oper.write = tfs_fuse_write;

  int ret = fuse_main(argc, argv + 1, &tfs_fuse_oper, NULL);

  return ret;
}
