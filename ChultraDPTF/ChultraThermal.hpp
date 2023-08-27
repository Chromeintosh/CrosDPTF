//
//  ChultraThermal.hpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#ifndef ChultraThermal_hpp
#define ChultraThermal_hpp

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOTimerEventSource.h>

#define DPTF_REGISTER_ZONE "DPTFRegisterZone"
#define DPTF_REGISTER_FAN "DPTFRegisterFan"
#define DPTF_REGISTER_SENSOR "DPTFRegisterSensor"

#define DPTF_UNREGISTER_ZONE "DPTFUnregisterZone"
#define DPTF_UNREGISTER_FAN "DPTFUnregisterFan"
#define DPTF_UNREGISTER_SENSOR "DPTFUnregisterSensor"

extern const OSSymbol *gDPTFRegisterZone;
extern const OSSymbol *gDPTFRegisterFan;
extern const OSSymbol *gDPTFRegisterSensor;

extern const OSSymbol *gDPTFUnregisterZone;
extern const OSSymbol *gDPTFUnregisterFan;
extern const OSSymbol *gDPTFUnregisterSensor;

enum {
    kIOMessageDptfSensorReadTemp = iokit_vendor_specific_msg(300),
    kIOMessageDptfSensorReadLevel = iokit_vendor_specific_msg(301),
    kIOMessageDptfFanSetLvl = iokit_vendor_specific_msg(302),
};

constexpr size_t DPTFActivePolicyMaxTemps = 10;

// Active Policy
struct DPTFActivePolicyEntry : public OSObject {
    OSDeclareDefaultStructors(DPTFActivePolicyEntry);
public:
    const OSSymbol *fan {nullptr};
    const OSSymbol *source {nullptr};
    uint32_t weight;
    uint32_t maxFanSpeeds[DPTFActivePolicyMaxTemps];
};

class ChultraThermal : public IOService {
    OSDeclareDefaultStructors(ChultraThermal);
public:
    bool init(OSDictionary *props) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;
    
    IOReturn callPlatformFunction(const OSSymbol *, bool, void *, void *, void *, void *) override;
    
    static LIBKERN_RETURNS_RETAINED ChultraThermal *WaitForThermal() {
        OSDictionary *matching = serviceMatching("ChultraThermal");
        if (matching == nullptr) return nullptr;
        
        IOService *service = waitForMatchingService(matching, 20000000000);
        ChultraThermal *ret = OSDynamicCast(ChultraThermal, service);
        if (ret == nullptr) {
            OSSafeReleaseNULL(service);
            return nullptr;
        }
        
        return ret;
    };
private:
    OSDictionary *fans {nullptr};
    OSDictionary *thermalZones {nullptr};
    OSDictionary *activePolicies {nullptr};
    OSDictionary *sensors {nullptr};
    
    IOWorkLoop *workloop {nullptr};
    IOTimerEventSource *timer {nullptr};
    
    IOReturn newState();
    IOReturn timerHandler(OSObject *, void *, void *, void *, void *);
};

#endif /* ChultraThermal_hpp */
