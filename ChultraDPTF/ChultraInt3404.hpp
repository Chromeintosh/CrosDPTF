//
//  ChultraInt3404.hpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#ifndef ChultraInt3404_hpp
#define ChultraInt3404_hpp

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>

#include "ChultraThermal.hpp"

class ChultraInt3404 : public IOService {
    OSDeclareDefaultStructors(ChultraInt3404);

    ChultraInt3404 *probe(IOService *provider, SInt32 *score) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    
    IOReturn message(uint32_t type, IOService *provider, void *args) override;
private:
    IOACPIPlatformDevice *acpi {nullptr};
    ChultraThermal *thermal {nullptr};
    
    bool fineGrainCtrl {false};
    bool underperformNotifs {false};
    
    uint32_t minStepSize {0};
    uint32_t lastState {0xFFFFFFFF};
    
    IOReturn parseFif();
    IOReturn setFanLevel(uint32_t level);
};

#endif /* ChultraInt3404_hpp */
