/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Provides the implementation of the Device Layer ConfigurationManager object
 *          for Zephyr platforms.
 */

#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <platform/ConfigurationManager.h>
#include <platform/internal/GenericConfigurationManagerImpl.cpp>

#include <lib/core/CHIPVendorIdentifiers.hpp>
#include <platform/Zephyr/ZephyrConfig.h>

#if CHIP_DEVICE_CONFIG_ENABLE_FACTORY_PROVISIONING
#include <platform/internal/FactoryProvisioning.cpp>
#endif // CHIP_DEVICE_CONFIG_ENABLE_FACTORY_PROVISIONING

#include <lib/support/CodeUtils.h>
#include <lib/support/logging/CHIPLogging.h>

#include <power/reboot.h>

namespace chip {
namespace DeviceLayer {

using namespace ::chip::DeviceLayer::Internal;

ConfigurationManagerImpl & ConfigurationManagerImpl::GetDefaultInstance()
{
    static ConfigurationManagerImpl sInstance;
    return sInstance;
}

CHIP_ERROR ConfigurationManagerImpl::Init()
{
    CHIP_ERROR err;
    uint32_t rebootCount;
    bool failSafeArmed;

    // Initialize the generic implementation base class.
    err = Internal::GenericConfigurationManagerImpl<ZephyrConfig>::Init();
    SuccessOrExit(err);

    // TODO: Initialize the global GroupKeyStore object here
#if CHIP_DEVICE_CONFIG_ENABLE_FACTORY_PROVISIONING
    {
        FactoryProvisioning factoryProv;
        uint8_t * const kDeviceRAMStart = (uint8_t *) CONFIG_SRAM_BASE_ADDRESS;
        uint8_t * const kDeviceRAMEnd   = kDeviceRAMStart + CONFIG_SRAM_SIZE * 1024 - 1;

        // Scan device RAM for injected provisioning data and save to persistent storage if found.
        err = factoryProv.ProvisionDeviceFromRAM(kDeviceRAMStart, kDeviceRAMEnd);
        SuccessOrExit(err);
    }
#endif // CHIP_DEVICE_CONFIG_ENABLE_FACTORY_PROVISIONING

    if (ZephyrConfig::ConfigValueExists(ZephyrConfig::kCounterKey_RebootCount))
    {
        err = GetRebootCount(rebootCount);
        SuccessOrExit(err);

        // Do not increment reboot count if the value is going to overflow UINT16.
        err = StoreRebootCount(rebootCount < UINT16_MAX ? rebootCount + 1 : rebootCount);
        SuccessOrExit(err);
    }
    else
    {
        // The first boot after factory reset of the Node.
        err = StoreRebootCount(1);
        SuccessOrExit(err);
    }

    // If the fail-safe was armed when the device last shutdown, initiate a factory reset.
    if (GetFailSafeArmed(failSafeArmed) == CHIP_NO_ERROR && failSafeArmed)
    {
        ChipLogProgress(DeviceLayer, "Detected fail-safe armed on reboot; initiating factory reset");
        InitiateFactoryReset();
    }

    err = CHIP_NO_ERROR;

exit:
    return err;
}

CHIP_ERROR ConfigurationManagerImpl::GetRebootCount(uint32_t & rebootCount)
{
    return ReadConfigValue(ZephyrConfig::kCounterKey_RebootCount, rebootCount);
}

CHIP_ERROR ConfigurationManagerImpl::StoreRebootCount(uint32_t rebootCount)
{
    return WriteConfigValue(ZephyrConfig::kCounterKey_RebootCount, rebootCount);
}

void ConfigurationManagerImpl::InitiateFactoryReset()
{
    PlatformMgr().ScheduleWork(DoFactoryReset);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, bool & val)
{
    return ZephyrConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, uint32_t & val)
{
    return ZephyrConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValue(Key key, uint64_t & val)
{
    return ZephyrConfig::ReadConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValueStr(Key key, char * buf, size_t bufSize, size_t & outLen)
{
    return ZephyrConfig::ReadConfigValueStr(key, buf, bufSize, outLen);
}

CHIP_ERROR ConfigurationManagerImpl::ReadConfigValueBin(Key key, uint8_t * buf, size_t bufSize, size_t & outLen)
{
    return ZephyrConfig::ReadConfigValueBin(key, buf, bufSize, outLen);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, bool val)
{
    return ZephyrConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, uint32_t val)
{
    return ZephyrConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValue(Key key, uint64_t val)
{
    return ZephyrConfig::WriteConfigValue(key, val);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueStr(Key key, const char * str)
{
    return ZephyrConfig::WriteConfigValueStr(key, str);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueStr(Key key, const char * str, size_t strLen)
{
    return ZephyrConfig::WriteConfigValueStr(key, str, strLen);
}

CHIP_ERROR ConfigurationManagerImpl::WriteConfigValueBin(Key key, const uint8_t * data, size_t dataLen)
{
    return ZephyrConfig::WriteConfigValueBin(key, data, dataLen);
}

void ConfigurationManagerImpl::RunConfigUnitTest(void)
{
    ZephyrConfig::RunConfigUnitTest();
}

void ConfigurationManagerImpl::DoFactoryReset(intptr_t arg)
{
    ChipLogProgress(DeviceLayer, "Performing factory reset");
    const CHIP_ERROR err = PersistedStorage::KeyValueStoreMgrImpl().DoFactoryReset();

    if (err != CHIP_NO_ERROR)
        ChipLogError(DeviceLayer, "Factory reset failed: %s", ErrorStr(err));

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
    ThreadStackMgr().ErasePersistentInfo();
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD

#if CONFIG_REBOOT
    sys_reboot(SYS_REBOOT_WARM);
#endif
}

} // namespace DeviceLayer
} // namespace chip
