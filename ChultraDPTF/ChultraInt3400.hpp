//
//  ChultraSofHda/hpp
//  ChultraSOF
//
//  Created by Gwydien on 8/21/23/
//

#ifndef ChultraInt3400_hpp
#define ChultraInt3400_hpp

#include "ChultraThermal.hpp"

#include <IOKit/IOService.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/acpi/IOACPITypes.h>

// ACPI Replacement for now since AppleACPI is unable to parse device handles
const struct { const char *heatSource; const char *sensor; uint32_t weight; uint32_t samplingPeriod; } KLEDTrt[] = {
    { "/_SB/PCI0/TCPU", "/_SB/PCI0/TCPU", 100, 50 },
    { "/_SB/PCI0/TCPU", "/_SB/DPTF/TSR0", 100, 60 },
    { "/_SB/DPTF/TCHG", "/_SB/DPTF/TSR1", 100, 60 },
    { "/_SB/DPTF/TCHG", "/_SB/DPTF/TSR2", 100, 60 },
};

const struct { const char *fanDev; const char *source; uint32_t weight; uint32_t maxFanSpeed[DPTFActivePolicyMaxTemps]; } KLEDArt[] = {
    { "/_SB/DPTF/TFN1", "/_SB/PCI0/TCPU", 100, {0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} }, // CPU Critical
    { "/_SB/DPTF/TFN1", "/_SB/DPTF/TSR0", 100, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} }, // Charger
    { "/_SB/DPTF/TFN1", "/_SB/DPTF/TSR1", 100, {0x5A, 0x50, 0x46, 0x3C, 0x32, 0x28, 0x1E, 0x00, 0x00, 0x00} }, // CPU Active
    { "/_SB/DPTF/TFN1", "/_SB/DPTF/TSR2", 100, {0x64, 0x50, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00} }, // Wifi
};

// DPTF Policies
enum dptf_policies_t {
    DPTFActivePolicy = 0,
    DPTFPassivePolicy,
    DPTFCriticalPolicy,
    DPTFPolicyMax
};

const char *DPTFPolicyGuids[DPTFPolicyMax] = {
    "3A95C389-E4B8-4629-A526-C52C88626BAE",
    "42a441d6-ae6a-462b-a84b-4a8ce79027d3",
    "97C68AE7-15FA-499c-B8C9-5DA81D606E0A"
};

// Passive Policy
struct DPTFThermalRelationsTable {
    uint32_t revision;
    struct DPTFThermalRelationEntry {
        
    } entries[];
};

class ChultraInt3400 : public IOService {
    OSDeclareDefaultStructors(ChultraInt3400);
  
    bool init(OSDictionary *props) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;
    
    IOReturn message(UInt32 type, IOService *provider, void *args=0) override;
    ChultraInt3400 *probe(IOService *provider, SInt32 *score) override;
    
private:
    IOACPIPlatformDevice *acpi {nullptr};
    ChultraThermal *thermal {nullptr};
    
    IOReturn acpiReadActivePolicy();
    IOReturn acpiReadThermalRelations();
    IOReturn acpiGetSupportedPolicies();
    
    OSDictionary *activePolicies {nullptr};
    OSArray *thermalRelations {nullptr};
    uint32_t supportedPolicies {0};
};


#endif // ChultraInt3400_hpp
