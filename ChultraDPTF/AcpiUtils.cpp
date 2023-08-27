//
//  AcpiUtils.cpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#include "AcpiUtils.hpp"
#include "Logger.h"

//using namespace ChultraACPIUtils;

IOReturn ChultraACPIUtils::acpiGetUInt32(IOACPIPlatformDevice *acpi, const char *methodName, uint32_t *toFill) {
    OSObject *typeRet;
    IOReturn ret = acpi->evaluateObject(methodName, &typeRet);
    OSNumber *typeNum = OSDynamicCast(OSNumber, typeRet);
    
    if (ret != kIOReturnSuccess){
        return ret;
    }
    
    if (typeRet == nullptr || typeNum == nullptr) {
        return kIOReturnInvalid;
    }
    
    *toFill = typeNum->unsigned32BitValue();
    return kIOReturnSuccess;
}

const OSSymbol *ChultraACPIUtils::acpiGetPath(IOACPIPlatformDevice *acpi) {
    char buffer[256];
    int size = sizeof(buffer);
    
    bool ret = acpi->getPath(buffer, &size, gIOACPIPlane);
    if (!ret) return nullptr;
    
    // Path will begin with "IOACPIPlane:", get rid of the prefix
    return OSSymbol::withCString(buffer + strlen("IOACPIPlane:"));
}
