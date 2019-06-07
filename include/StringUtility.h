#pragma once

#include <string>
#include <vector>
#include <tuple>

namespace detail {
  std::tuple<bool, std::string> endsWith(std::string const &s,
      const std::vector<std::string> &patterns);

  long slashes(const std::string& s);

} // namespace detail
