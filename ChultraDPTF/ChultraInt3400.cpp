//
//  ChultraSofHda.cpp
//  ChultraSOF
//
//  Created by Gwydien on 8/21/23.
//

#include "ChultraInt3400.hpp"
#include "AcpiUtils.hpp"
#include "Logger.h"
#include <IOKit/IOLib.h>
#include <libkern/OSKextLib.h>
#include <Availability.h>

#ifndef __ACIDANTHERA_MAC_SDK
#error "MacKernelSDK is missing. Please make sure it is cloned!"
#endif // __ACIDANTHERA_MAC_SDK

#define super IOService
OSDefineMetaClassAndStructors(ChultraInt3400, IOService);
OSDefineMetaClassAndStructors(DPTFActivePolicyEntry, OSObject);

bool ChultraInt3400::init(OSDictionary *props) {
    if (!super::init(props)) {
        return false;
    }
    
    activePolicies = OSDictionary::withCapacity(1);
    if (activePolicies == nullptr) {
        return false;
    }
    
    thermalRelations = OSArray::withCapacity(4);
    if (thermalRelations == nullptr) {
        OSSafeReleaseNULL(activePolicies);
        return false;
    }
    
    return true;
}

ChultraInt3400 *ChultraInt3400::probe(IOService *provider, SInt32 *score) {
    IOLogInfo("Driver startup!");
    
    if (super::probe(provider, score) == nullptr) {
        IOLogError("Failed to probe");
        return nullptr;
    }
    
    acpi = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (acpi == nullptr) {
        IOLogError("Nub is not PCI device");
        return nullptr;
    }

    //
    // Get capabilities from ACPI
    // For now, we only support active policies
    //
    
    if (acpiGetSupportedPolicies() != kIOReturnSuccess) {
        IOLogError("Failed to read supported policies");
        return nullptr;
    }
    
    //
    // Get active policy data so we know fan curve
    //
    
    if (acpiReadActivePolicy() != kIOReturnSuccess) {
        IOLogError("Failed to read active polcy");
        return nullptr;
    }
    
    return this;
}

bool ChultraInt3400::start(IOService *provider) {
    const OSSymbol *acpiPath;
    IOReturn ret;
    
    thermal = ChultraThermal::WaitForThermal();
    acpiPath = ChultraACPIUtils::acpiGetPath(acpi);
    
    if (thermal == nullptr) {
        return false;
    }
    
    if (acpiPath == nullptr) {
        goto err;
    }
        
    ret = thermal->callPlatformFunction(gDPTFRegisterZone, true, (void *) acpiPath, this, activePolicies, nullptr);
    if (ret != kIOReturnSuccess) {
        goto err;
    }
    
    registerService();
    return super::start(provider);
err:
    stop(provider);
    return false;
}

void ChultraInt3400::stop(IOService *provider) {
    if (thermal != nullptr) {
        const OSSymbol *acpiPath = ChultraACPIUtils::acpiGetPath(acpi);
        (void) thermal->callPlatformFunction(gDPTFUnregisterZone, true, (void *) acpiPath, nullptr, nullptr, nullptr);
        OSSafeReleaseNULL(thermal);
    }
    
    return super::stop(provider);
}

void ChultraInt3400::free() {
    OSSafeReleaseNULL(activePolicies);
    OSSafeReleaseNULL(thermalRelations);
    
    super::free();
}

IOReturn ChultraInt3400::acpiReadActivePolicy() {
    
    // TODO: Fix AppleACPI being unable to parse this data
#if 0
    OSObject *artReturn;
    
    IOReturn ret = acpi->evaluateObject("_ART", &artReturn);
    if (ret != kIOReturnSuccess) {
        return ret;
    }
    
    OSArray *artPolicies = OSDynamicCast(OSArray, artReturn);
    if (artPolicies == nullptr) {
        IOLogError("Could not cast _ART return");
        return kIOReturnInvalid;
    }
    
    // First entry is revision
    OSNumber *revision = OSDynamicCast(OSNumber, artPolicies->getObject(0));
    if (revision == nullptr) {
        IOLogError("Could not get revision");
        return kIOReturnInvalid;
    }
    
    IOLogInfo("Parsing Active Policy revision %d", revision->unsigned32BitValue());
    
    // All other entries provide fan curves and thermal relations
    for (size_t i = 1; i < artPolicies->getCount(); i++) {
        DPTFActivePolicyEntry policy;
        
        OSArray *rawPolicy = OSDynamicCast(OSArray, artPolicies->getObject((uint32_t) i));
        if (rawPolicy == nullptr) {
            continue;
        }
        
        IOLogInfo("Type = %s", rawPolicy->getObject(1)->getMetaClass()->getClassName());
    }
#endif
    
    // While ACPI is borked, parse our table into the right structs instead
    for (int i = 0; i < 4; i++) {
        DPTFActivePolicyEntry *entry = new DPTFActivePolicyEntry();
        if (entry == nullptr) return kIOReturnNoMemory;
        
        entry->fan = OSSymbol::withCString(KLEDArt[i].fanDev);
        entry->source = OSSymbol::withCString(KLEDArt[i].source);
        entry->weight = KLEDArt[i].weight;
        memcpy(entry->maxFanSpeeds, KLEDArt[i].maxFanSpeed, sizeof(entry->maxFanSpeeds));
        
        // Sort zone entries by fan
        OSDictionary *fanDict = OSDynamicCast(OSDictionary, activePolicies->getObject(entry->fan));
        if (fanDict == nullptr) {
            fanDict = OSDictionary::withCapacity(1);
            activePolicies->setObject(entry->fan, fanDict);
            fanDict->release();
        }
        
        fanDict->setObject(entry->source, entry);
        entry->release();
    }
    
    return kIOReturnSuccess;
}

IOReturn ChultraInt3400::acpiReadThermalRelations() {
    return kIOReturnSuccess;
}

IOReturn ChultraInt3400::acpiGetSupportedPolicies() {
    OSObject *idspReturn;
    
    IOReturn ret = acpi->evaluateObject("IDSP", &idspReturn);
    if (ret != kIOReturnSuccess) {
        IOLogInfo("Failed to grab supported GUIDs (Not a failure)");
        return kIOReturnSuccess;
    }
    
    OSArray *supportedGuids = OSDynamicCast(OSArray, idspReturn);
    if (supportedGuids == nullptr) {
        OSSafeReleaseNULL(idspReturn);
        return kIOReturnInvalid;
    }
    
    //
    // Turn GUIDs into format that can be compared
    // Then compare with known GUIDs
    //
    
    for (size_t i = 0; i < supportedGuids->getCount(); i++) {
        uuid_t guid;
        OSData *entry = OSDynamicCast(OSData, supportedGuids->getObject((unsigned int) i));
        if (entry == nullptr) {
            continue;
        }
        
        memcpy(guid, entry->getBytesNoCopy(), sizeof(uuid_t));
        *(reinterpret_cast<uint32_t *>(guid)) = OSSwapInt32(*(reinterpret_cast<uint32_t *>(guid)));
        *(reinterpret_cast<uint16_t *>(guid) + 2) = OSSwapInt16(*(reinterpret_cast<uint16_t *>(guid) + 2));
        *(reinterpret_cast<uint16_t *>(guid) + 3) = OSSwapInt16(*(reinterpret_cast<uint16_t *>(guid) + 3));
        
        for (size_t pol = 0; pol < DPTFPolicyMax; pol++) {
            uuid_t knownGuid;
            if (uuid_parse(DPTFPolicyGuids[pol], knownGuid) < 0) {
                continue;
            }

            if (uuid_compare(guid, knownGuid) == 0) {
                supportedPolicies |= (1 << pol);
                break;
            }
        }
    }
    
    OSSafeReleaseNULL(idspReturn);
    return kIOReturnSuccess;
}

IOReturn ChultraInt3400::message(UInt32 type, IOService *provider, void *args) {
    switch (type) {
        case kIOACPIMessageDeviceNotification:
            if (thermal != nullptr) {
                thermal->message(type, provider, args);
            }
            break;
        default:
            return super::message(type, provider, args);
    }
    
    return kIOReturnSuccess;
}
