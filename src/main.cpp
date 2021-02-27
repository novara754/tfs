#include "tfs.h"
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

auto get_instance() -> tfs::tfs_instance {
  return *reinterpret_cast<tfs::tfs_instance *>(
      fuse_get_context()->private_data);
}

void read_at(int fd, off_t off, void *buf, size_t len) {
  lseek(fd, off, 0);
  read(fd, buf, len);
}

void *tfs_fuse_init(struct fuse_conn_info *conn, struct fuse_config *conf) {
  UNUSED(conn);
  UNUSED(conf);

  auto tfs = new tfs::tfs_instance{};

  uint8_t buf[1];
  read_at(source_fd, 509, &buf, 1);
  tfs->reserved_blocks = buf[0];

  return tfs;
}

void tfs_fuse_destroy(void *private_data) {
  auto tfs = reinterpret_cast<tfs::tfs_instance *>(private_data);
  delete tfs;
  close(source_fd);
  fclose(log);
}

int tfs_fuse_getattr(const char *path, struct stat *st,
                     struct fuse_file_info *fi) {
  UNUSED(fi);

  fprintf(log, "now looking for path=%s\n", path);

  auto instance = get_instance();

  auto next_slash = strchr(path + 1, '/');
  auto disk_offset = tfs::get_data_block_offset(instance, 0);

  while (next_slash != NULL) {
    const char *segment = path + 1;
    size_t segment_len;
    if (next_slash == NULL) {
      segment_len = strlen(segment);
    } else {
      segment_len = next_slash - path - 1;
    }

    fprintf(log, "segment=%.*s\n", (int)segment_len, segment);

    tfs::dir_ent buf[tfs::BLOCK_SIZE / sizeof(tfs::dir_ent)];
    read_at(source_fd, disk_offset, buf, tfs::BLOCK_SIZE);

    auto entry = tfs::find_dir_ent({segment, segment_len}, std::begin(buf),
                                   std::end(buf));
    if (entry == nullptr) {
      return -ENOTDIR;
    }

    if (next_slash != NULL && !entry->is_dir()) {
      return -ENOENT;
    }

    disk_offset = tfs::get_data_block_offset(instance, entry->data.start_block);
    path = next_slash;
    next_slash = strchr(segment, '/');
  }

  // `disk_offset` is now set to the location of the parent directory
  // for the requested file.
  // `path` now points to the file's basename.
  const char *filename = path + 1;
  if (filename[0] == 0) {
    filename = ".";
  }

  fprintf(log, "filename=%s, disk_offset=%ld\n", filename, disk_offset);

  tfs::dir_ent buf[tfs::BLOCK_SIZE / sizeof(tfs::dir_ent)];
  read_at(source_fd, disk_offset, buf, tfs::BLOCK_SIZE);

  auto entry = tfs::find_dir_ent(filename, std::begin(buf), std::end(buf));
  if (entry == nullptr) {
    return -ENOENT;
  }

  fprintf(log, "entry->is_dir=%d\n", entry->is_dir());
  st->st_mode = entry->is_dir() ? __S_IFDIR : __S_IFREG;

  fprintf(log, "now returning 0 from getattr\n");

  return 0;
}

int tfs_fuse_readdir(const char *path, void *dir_buf, fuse_fill_dir_t filler,
                     off_t off, struct fuse_file_info *fi,
                     enum fuse_readdir_flags flags) {
  UNUSED(off);
  UNUSED(fi);
  UNUSED(flags);

  auto instance = get_instance();

  if (strcmp(path, "/") == 0) {
    uint8_t buf[tfs::BLOCK_SIZE];
    read_at(source_fd, tfs::get_data_block_offset(instance, 0), buf,
            tfs::BLOCK_SIZE);

    filler(dir_buf, "..", NULL, 0, static_cast<fuse_fill_dir_flags>(0));

    for (size_t i = 0; i < tfs::BLOCK_SIZE; i += sizeof(tfs::dir_ent)) {
      auto entry = (tfs::dir_ent *)&buf[i];
      auto type = entry->get_type();
      if (type == tfs::dir_ent::type::END) {
        break;
      }
      if (type == tfs::dir_ent::type::EMPTY) {
        continue;
      }

      filler(dir_buf, entry->clean_name().c_str(), NULL, 0,
             static_cast<fuse_fill_dir_flags>(0));
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
