#include "tfs.h"
#include "util.h"

namespace tfs {

auto tfs_instance::get_block_offset(std::size_t idx) -> std::size_t {
  return idx * BLOCK_SIZE;
}

auto tfs_instance::get_data_block_offset(std::size_t idx) -> std::size_t {
  return (this->reserved_blocks + 1) * BLOCK_SIZE + BLOCK_USAGE_BITMAP_SIZE +
         idx * BLOCK_SIZE;
}

auto tfs_instance::get_dir_ent_for_path(std::string_view path)
    -> std::optional<dir_ent> {
  auto disk_offset = this->get_data_block_offset(0);
  auto segments = util::split_by(path, '/');
  for (std::size_t i = 0; i < segments.size() - 1; i++) {
    const auto &segment = segments[i];

    tfs::dir_ent buf[tfs::BLOCK_SIZE / sizeof(tfs::dir_ent)];
    this->read_at(disk_offset, buf, tfs::BLOCK_SIZE);

    auto entry = tfs::find_dir_ent(segment, std::begin(buf), std::end(buf));
    if (!entry.has_value()) {
      return {};
    }

    if (i != (segments.size() - 2) && !entry->is_dir()) {
      return {};
    }

    disk_offset = this->get_data_block_offset(entry->data.start_block);
  }

  // `disk_offset` is now set to the location of the parent directory
  // for the requested file.

  auto filename = segments.back();
  if (filename.size() == 0) {
    filename = ".";
  }

  tfs::dir_ent buf[tfs::BLOCK_SIZE / sizeof(tfs::dir_ent)];
  this->read_at(disk_offset, buf, tfs::BLOCK_SIZE);

  return find_dir_ent(filename, std::begin(buf), std::end(buf));
}

auto tfs_instance::read_dir(std::string_view path)
    -> std::optional<std::vector<dir_ent>> {
  auto dir = this->get_dir_ent_for_path(path);
  if (!dir.has_value()) {
    return {};
  }

  tfs::dir_ent buf[tfs::BLOCK_SIZE / sizeof(tfs::dir_ent)];
  this->read_at(this->get_data_block_offset(dir->data.start_block), buf,
                tfs::BLOCK_SIZE);

  std::vector<dir_ent> entries;
  for (const auto &entry : buf) {
    if (entry.get_type() == dir_ent::type::END) {
      break;
    }
    if (entry.get_type() == dir_ent::type::EMPTY) {
      continue;
    }
    entries.push_back(entry);
  }

  return entries;
}

auto tfs_instance::read_file(std::string_view path)
    -> std::optional<std::vector<std::uint8_t>> {
  auto file = this->get_dir_ent_for_path(path);
  if (!file.has_value()) {
    return {};
  }

  std::vector<std::uint8_t> data;
  data.resize(file->total_size());
  this->read_at(this->get_data_block_offset(file->data.start_block),
                data.data(), file->total_size());

  return data;
}

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

auto dir_ent::total_size() const -> std::size_t {
  return (this->data.num_blocks - 1) * BLOCK_SIZE + this->data.size;
}

} // namespace tfs
