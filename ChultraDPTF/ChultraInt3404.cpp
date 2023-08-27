//
//  ChultraInt3404.cpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#include "ChultraInt3404.hpp"
#include "AcpiUtils.hpp"
#include "Logger.h"

#define super IOService
OSDefineMetaClassAndStructors(ChultraInt3404, IOService);

#define abs(v) (((v) < 0) ? -(v) : (v))

ChultraInt3404 *ChultraInt3404::probe(IOService *provider, SInt32 *score) {
    IOReturn ret;
    
    acpi = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (acpi == nullptr) {
        return nullptr;
    }
    
    //
    // Check for fine grained control
    // If it exists, then we have 100 states
    // If not, then there are sizeof(_FPS) states
    //
    
    ret = parseFif();
    if (ret != kIOReturnNoDevice && ret != kIOReturnSuccess) {
        IOLogError("Error reading _FIF");
        return nullptr;
    }
    
    // TODO: We should probably read _FPS no matter what
    // Going to ignore for now since my device has fine grain control
    if (!fineGrainCtrl) {
        return nullptr;
    }
    
    return super::probe(provider, score) ? this : nullptr;
}

bool ChultraInt3404::start(IOService *provider) {
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
    
    ret = thermal->callPlatformFunction(gDPTFRegisterFan, true, (void *) acpiPath, this, nullptr, nullptr);
    if (ret != kIOReturnSuccess) {
        goto err;
    }
    
    registerService();
    return super::start(provider);
err:
    stop(provider);
    return false;
}

void ChultraInt3404::stop(IOService *provider) {
    if (thermal != nullptr) {
        const OSSymbol *acpiPath = ChultraACPIUtils::acpiGetPath(acpi);
        (void) thermal->callPlatformFunction(gDPTFUnregisterFan, true, (void *) acpiPath, nullptr, nullptr, nullptr);
        OSSafeReleaseNULL(thermal);
    }
    super::stop(provider);
}

IOReturn ChultraInt3404::message(uint32_t type, IOService *provider, void *args) {
    uint32_t *newLevel = static_cast<uint32_t *>(args);
    
    switch (type) {
        case kIOMessageDptfFanSetLvl:
            return setFanLevel(*newLevel);
        default:
            return super::message(type, provider, args);
    }
    
    return kIOReturnSuccess;
}

IOReturn ChultraInt3404::parseFif() {
    OSObject *acpiRet;
    OSArray *_fifArray;
    IOReturn ret;
    
    ret = acpi->evaluateObject("_FIF", &acpiRet);
    if (ret != kIOReturnSuccess) {
        return kIOReturnNoDevice;
    }
    
    _fifArray = OSDynamicCast(OSArray, acpiRet);
    if (_fifArray == nullptr || _fifArray->getCount() != 4) {
        IOLogError("Invalid _FIF package!");
        return kIOReturnInvalid;
    }
    
    if (OSNumber *num = OSDynamicCast(OSNumber, _fifArray->getObject(1))) {
        fineGrainCtrl = num->unsigned32BitValue() != 0;
    }
    
    if (OSNumber *num = OSDynamicCast(OSNumber, _fifArray->getObject(2))) {
        minStepSize = num->unsigned32BitValue();
    }
    
    if (OSNumber *num = OSDynamicCast(OSNumber, _fifArray->getObject(3))) {
        underperformNotifs = num->unsigned32BitValue() != 0;
    }
    
    return kIOReturnSuccess;
}

IOReturn ChultraInt3404::setFanLevel(uint32_t level) {
    if (fineGrainCtrl) {
        if (abs(lastState - level) < minStepSize) {
            return kIOReturnSuccess;
        }
    }
    
    OSNumber *acpiLevel = OSNumber::withNumber(level, 32);
    if (acpiLevel == nullptr) {
        return kIOReturnNoMemory;
    }
    
    OSObject *params[1] = {
        acpiLevel,
    };
    
    return acpi->evaluateObject("_FSL", nullptr, params, 1);
}
