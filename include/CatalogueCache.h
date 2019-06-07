#pragma once

#include <ChimeraTK/RegisterCatalogue.h>

#include <memory>
#include <string>

namespace Cache {
  std::unique_ptr<ChimeraTK::RegisterCatalogue> readCatalogue(const std::string& xmlfile);
  void saveCatalogue(ChimeraTK::RegisterCatalogue& c, const std::string& xmlfile);
} // namespace Cache
