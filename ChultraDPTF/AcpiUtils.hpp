//
//  AcpiUtils.hpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#ifndef AcpiUtils_hpp
#define AcpiUtils_hpp

#include <IOKit/IOLib.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <stdint.h>

namespace ChultraACPIUtils {
    typedef uint32_t celsius_t;

    // ACPI reports tenths of degrees Kelvin (xx.x)
    inline celsius_t acpiTempToCelsius(uint32_t kelvin) {
        return kelvin - 2732; //273.15 rounded up
    }

    IOReturn acpiGetUInt32(IOACPIPlatformDevice *acpi, const char *methodName, uint32_t *toFill);
    const OSSymbol *acpiGetPath(IOACPIPlatformDevice *acpi);
}

#endif /* AcpiUtils_hpp */
