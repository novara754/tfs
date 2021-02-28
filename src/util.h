#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <vector>

namespace util {

auto split_by(std::string_view s, char delim) -> std::vector<std::string_view>;

} // namespace util

#endif // UTIL_H
