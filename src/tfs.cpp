#include "tfs.h"
#include <string.h>

namespace tfs {

auto dir_ent::clean_name() const -> std::string {
  std::string clean = reinterpret_cast<const char *>(this->data.name);
  clean[0] &= 0x7F;
  return clean;
}

auto dir_ent::is_dir() const -> bool {
  return (this->data.name[0] & 0x80) != 0;
}

auto dir_ent::is_regular_file() const -> bool {
  return !this->is_dir();
}

auto dir_ent::get_type() const -> dir_ent::type {
  auto marks_end = true;
  auto empty = true;

  for (const auto &d : this->raw_data) {
    if (d != 0x00) {
      marks_end = false;
    }
    if (d != 0x7F) {
      empty = false;
    }
  }

  if (marks_end) {
    return dir_ent::type::END;
  }
  if (empty) {
    return dir_ent::type::EMPTY;
  }
  return dir_ent::type::USED;
}

auto get_block_offset(std::size_t idx) -> std::size_t {
  return idx * BLOCK_SIZE;
}

auto get_data_block_offset(const tfs_instance &instance, std::size_t idx)
    -> std::size_t {
  return (instance.reserved_blocks + 1) * BLOCK_SIZE + BLOCK_USAGE_BITMAP_SIZE +
         idx * BLOCK_SIZE;
}

} // namespace tfs
