//
//  ChultraInt3403.hpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#ifndef ChultraInt3403_hpp
#define ChultraInt3403_hpp

#include <IOKit/IOService.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>

#include "ChultraThermal.hpp"
#include "AcpiUtils.hpp"

enum ACParseState_t {
    ACParseHighestTemp = 0,
    ACParseLowestTemp = 10,
};

class ChultraInt3403 : public IOService {
    OSDeclareDefaultStructors(ChultraInt3403);

    ChultraInt3403 *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    IOReturn message(uint32_t type, IOService *provider, void *args) override;
private:
    
    enum ChultraInt3403Type {
        Sensor,
        Charger
    };
    
    ChultraInt3403Type type;
    IOACPIPlatformDevice *acpi {nullptr};
    ChultraACPIUtils::celsius_t activeTripPoints[ACParseLowestTemp];
    ChultraACPIUtils::celsius_t hysteresis {0};
    ChultraThermal *thermal {nullptr};
    
    // Start at lowest state until we first read temp
    uint32_t lastState {ACParseLowestTemp};
    
    IOReturn getTemp(uint32_t *);
    IOReturn getThermalState(uint32_t *);
    IOReturn parseACx();
};

#endif /* ChultraInt3403_hpp */
