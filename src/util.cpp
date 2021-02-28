#include "util.h"
#include <algorithm>

namespace util {

auto split_by(std::string_view s, char delim) -> std::vector<std::string_view> {
  std::vector<std::string_view> segments;
  segments.reserve(std::count(s.begin(), s.end(), '/'));

  auto current = s.find(delim);
  std::size_t previous = 0;
  while (current != std::string::npos) {
    auto token = s.substr(previous, current - previous);
    if (token.size() > 0) {
      segments.push_back(token);
    }

    previous = current + 1;
    current = s.find(delim, previous);
  }

  segments.push_back(s.substr(previous, current - previous));

  return segments;
}

} // namespace util
