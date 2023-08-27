//
//  ChultraInt3403.cpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#include "ChultraInt3403.hpp"
#include "AcpiUtils.hpp"
#include "Logger.h"

#define super IOService
OSDefineMetaClassAndStructors(ChultraInt3403, IOService);

ChultraInt3403 *ChultraInt3403::probe(IOService *provider, SInt32 *score) {
    acpi = OSDynamicCast(IOACPIPlatformDevice, provider);
    if (acpi == nullptr) {
        return nullptr;
    }
    
    uint32_t typeRet;
    IOReturn err = ChultraACPIUtils::acpiGetUInt32(acpi, "PTYP", &typeRet);
    if (err != kIOReturnSuccess) {
        return nullptr;
    }
    
    switch (typeRet) {
        case 3: type = Sensor; break;
        case 11: type = Charger; break;
        default: return nullptr;
    }
    
    // Not needed for fan control
    // TODO: Enable for passive thermal control!
    if (type == Charger) return nullptr;
    
    return super::probe(provider, score) ? this : nullptr;
}

bool ChultraInt3403::start(IOService *provider) {
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
    
    // TODO: Check errors here
    if (type == Sensor) {
        ret = parseACx();
        if (ret != kIOReturnSuccess) {
            goto err;
        }
        
        ret = thermal->callPlatformFunction(gDPTFRegisterSensor, true, (void *) acpiPath, this, nullptr, nullptr);
        if (ret != kIOReturnSuccess) {
            goto err;
        }
        
    } else {
        // TODO: Parse charger power levels for passive policy
    }
    
    registerService();
    return super::start(provider);
err:
    stop(provider);
    return false;
}

void ChultraInt3403::stop(IOService *provider) {
    if (thermal != nullptr) {
        const OSSymbol *acpiPath = ChultraACPIUtils::acpiGetPath(acpi);
        (void) thermal->callPlatformFunction(gDPTFUnregisterSensor, true, (void *) acpiPath, nullptr, nullptr, nullptr);
        OSSafeReleaseNULL(thermal);
    }
    super::stop(provider);
}

IOReturn ChultraInt3403::message(uint32_t type, IOService *provider, void *args) {
    uint32_t *toFill = static_cast<uint32_t *>(args);
    
    switch (type) {
        case kIOMessageDptfSensorReadLevel:
            return getThermalState(toFill);
        case kIOMessageDptfSensorReadTemp:
            return getTemp(toFill);
        default:
            return super::message(type, provider, args);
    }
    
    return kIOReturnSuccess;
}

IOReturn ChultraInt3403::getTemp(uint32_t *toFill) {
    uint32_t acpiRet;
    IOReturn err = ChultraACPIUtils::acpiGetUInt32(acpi, "_TMP", &acpiRet);
    ChultraACPIUtils::celsius_t temp = ChultraACPIUtils::acpiTempToCelsius(acpiRet);
    
    if (err == kIOReturnSuccess) {
        IOLogDebug("\t\t\t%s - Temp is current: %d.%d", acpi->getName(), temp / 10, temp % 10);
    }
    
    *toFill = temp;
    return err;
}

IOReturn ChultraInt3403::getThermalState(uint32_t *toFill) {
    uint32_t temp;
    IOReturn err = getTemp(&temp);
    if (err != kIOReturnSuccess) return err;
    
    // Walk through states from highest temp to lowest temp
    for (size_t i = ACParseHighestTemp; i < ACParseLowestTemp; i++) {
        // Lowest speed, turn off fan
        if (activeTripPoints[i] == 0) {
            *toFill = ACParseLowestTemp;
            break;
        }
        
        // When downgrading states, take into account hysteresis
        if (i == lastState) temp += hysteresis;
        
        // Highest state where we trip
        if (temp > activeTripPoints[i]) {
            *toFill = (uint32_t) i;
            break;
        }
    }
    
    lastState = *toFill;
    return kIOReturnSuccess;
}

IOReturn ChultraInt3403::parseACx() {
    IOReturn err;
    uint32_t temp;
    char methodName[5] = "_AC0";
    
    // Grab Hysteresis value for downgrading state
    (void) ChultraACPIUtils::acpiGetUInt32(acpi, "GTSH", &hysteresis);
    
    // Read all the _ACx methods to get trip points for active policy.
    // These give us temperatures at which we should increase fan speed.
    // 0 is the highest fan speed, while 1-9 are increasing slower.

    bzero(activeTripPoints, sizeof(activeTripPoints));
    for (size_t i = ACParseHighestTemp; i < ACParseLowestTemp; i++) {
        methodName[3] = '0' + i;
        err = ChultraACPIUtils::acpiGetUInt32(acpi, methodName, &temp);
        
        // Not all 10 methods are guaranteed to be here.
        // Can't really differentiate between no method and other errors,
        // So hopefully everything is ok!
        if (err != kIOReturnSuccess) break;
        
        activeTripPoints[i] = ChultraACPIUtils::acpiTempToCelsius(temp) + 130;
    }
    
    // Always return success for now
    // There *can* be zero _AC states and no hystersis
    return kIOReturnSuccess;
}
