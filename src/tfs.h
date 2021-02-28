#ifndef TFS_H
#define TFS_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace tfs {

constexpr std::size_t BLOCK_SIZE = 1024;
constexpr std::size_t BLOCK_USAGE_BITMAP_SIZE = BLOCK_SIZE * 2;

struct dir_ent;

class tfs_instance {
private:
  std::uint8_t reserved_blocks;
  std::function<void(off_t off, void *buf, size_t len)> read_at;

public:
  tfs_instance(auto read_at) : read_at(read_at) {
    std::uint8_t buf[1];
    this->read_at(509, &buf, 1);
    this->reserved_blocks = buf[0];
  }

  /**
   * Calculate the byte offset for a given block index.
   */
  auto get_block_offset(std::size_t idx) -> std::size_t;

  /**
   * Calculate the byte offset for a given data block index
   * while respecting the number of configured reserved blocks
   * at the start.
   */
  auto get_data_block_offset(std::size_t idx) -> std::size_t;

  /**
   * Get a `dir_ent` corresponding to the given absolute path.
   */
  auto get_dir_ent_for_path(std::string_view path) -> std::optional<dir_ent>;
};

struct __attribute__((packed)) dir_ent {
  union {
    struct {
      std::uint8_t name[10];
      std::uint16_t start_block;
      std::uint16_t num_blocks;
      std::uint16_t size;
    } data;
    std::uint8_t raw_data[16];
  };

  enum type {
    USED,
    EMPTY,
    END,
  };

  /**
   * Get a clean version of the filename, removing the
   * DIR indicator bit.
   */
  auto clean_name() const -> std::string;

  /**
   * Whether this directory entry belongs to a directory.
   */
  auto is_dir() const -> bool;

  /**
   * Whether this directory entry belong to a regular file.
   */
  auto is_regular_file() const -> bool;

  /**
   * Get the state of the given directory entry.
   *
   * USED    - A used directory entry.
   * EMPTY   - An empty directory entry.
   * END     - An empty directory entry marking the end of the
   * directory.
   */
  auto get_type() const -> type;
};

/**
 * Find a file or directory entry in a directory block by its name.
 *
 * Return NULL if entry not found.
 */
template<typename Iter>
std::optional<dir_ent> find_dir_ent(std::string_view name, Iter start,
                                    Iter end) {
  auto res = std::find_if(start, end, [&name](auto entry) {
    auto type = entry.get_type();
    if (type != dir_ent::type::USED) {
      return false;
    }

    return entry.clean_name() == name;
  });

  return res != end ? *res : std::optional<dir_ent>{};
}

} // namespace tfs

#endif // TFS_H
