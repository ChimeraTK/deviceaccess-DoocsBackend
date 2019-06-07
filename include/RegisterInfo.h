#pragma once

#include <ChimeraTK/RegisterInfo.h>

/**
 *  RegisterInfo-derived class to be put into the RegisterCatalogue
 */
  class DoocsBackendRegisterInfo;

  using RegisterInfoList = std::vector<boost::shared_ptr<DoocsBackendRegisterInfo>>;

  class DoocsBackendRegisterInfo : public ChimeraTK::RegisterInfo {
   private:
    DoocsBackendRegisterInfo() = default;

   public:
    DoocsBackendRegisterInfo(const DoocsBackendRegisterInfo& other) = default;

    ~DoocsBackendRegisterInfo() override {}

    ChimeraTK::RegisterPath getRegisterName() const override { return _name; }

    unsigned int getNumberOfElements() const override { return _length; }

    unsigned int getNumberOfChannels() const override { return 1; }

    unsigned int getNumberOfDimensions() const override { return _length > 1 ? 1 : 0; }

    bool isReadable() const override { return true; }

    bool isWriteable() const override { return true; } /// @todo fixme: return false for read-only properties

    ChimeraTK::AccessModeFlags getSupportedAccessModes() const override { return accessModeFlags; }

    const ChimeraTK::RegisterInfo::DataDescriptor& getDataDescriptor() const override { return dataDescriptor; }

    static RegisterInfoList create(const std::string& name, unsigned int length, int doocsType);

    ChimeraTK::RegisterPath _name;
    unsigned int _length;
    ChimeraTK::RegisterInfo::DataDescriptor dataDescriptor;
    ChimeraTK::AccessModeFlags accessModeFlags{};
    int doocsTypeId;
  };
