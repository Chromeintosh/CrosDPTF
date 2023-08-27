//
//  ChultraThermal.cpp
//  ChultraDPTF
//
//  Created by Gwydien on 8/26/23.
//

#include "ChultraThermal.hpp"
#include "Logger.h"

#define super IOService
OSDefineMetaClassAndStructors(ChultraThermal, IOService);

#define max(a, b) ((a) > (b) ? (a) : (b))

const OSSymbol *gDPTFRegisterZone = nullptr;
const OSSymbol *gDPTFRegisterFan = nullptr;
const OSSymbol *gDPTFRegisterSensor = nullptr;

const OSSymbol *gDPTFUnregisterZone = nullptr;
const OSSymbol *gDPTFUnregisterFan = nullptr;
const OSSymbol *gDPTFUnregisterSensor = nullptr;

bool ChultraThermal::init(OSDictionary *props) {
    if (!super::init(props)) {
        return false;
    }
    
    gDPTFRegisterZone = OSSymbol::withCString(DPTF_REGISTER_ZONE);
    gDPTFRegisterFan = OSSymbol::withCString(DPTF_REGISTER_FAN);
    gDPTFRegisterSensor = OSSymbol::withCString(DPTF_REGISTER_SENSOR);
    
    gDPTFUnregisterZone = OSSymbol::withCString(DPTF_UNREGISTER_ZONE);
    gDPTFUnregisterFan = OSSymbol::withCString(DPTF_UNREGISTER_FAN);
    gDPTFUnregisterSensor = OSSymbol::withCString(DPTF_UNREGISTER_SENSOR);
    
    fans = OSDictionary::withCapacity(1);
    thermalZones = OSDictionary::withCapacity(1);
    sensors = OSDictionary::withCapacity(1);
    activePolicies = OSDictionary::withCapacity(1);
    
    return true ;
}

bool ChultraThermal::start(IOService *provider) {
    if (!super::start(provider)) {
        return false;
    }
    
    workloop = IOWorkLoop::workLoop();
    timer = IOTimerEventSource::timerEventSource(this);
    
    if (workloop == nullptr || timer == nullptr) {
        return false;
    }
    
    workloop->addEventSource(timer);
    timer->setAction(OSMemberFunctionCast(IOEventSourceAction, this, &ChultraThermal::timerHandler));
    timer->enable();
    timer->setTimeoutMS(10000);
    
    registerService();
    return true;
}

void ChultraThermal::stop(IOService *provider) {
    timer->cancelTimeout();
    timer->disable();
    
    super::stop(provider);
}

void ChultraThermal::free() {
    OSSafeReleaseNULL(fans);
    OSSafeReleaseNULL(thermalZones);
    OSSafeReleaseNULL(sensors);
    OSSafeReleaseNULL(activePolicies);
    
    if (workloop && timer) {
        workloop->removeEventSource(timer);
    }
    
    OSSafeReleaseNULL(workloop);
    OSSafeReleaseNULL(timer);
    
    super::free();
}

IOReturn ChultraThermal::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction, void *param1, void *param2, void *param3, void *param4) {
    const OSSymbol *acpiPath = static_cast<const OSSymbol *>(param1);
    IOService *service = static_cast<IOService *>(param2);
    
    // Thermal Zones
    if (functionName == gDPTFRegisterZone) {
        // Fan Dev -> Source -> Active Policies
        OSDictionary *zonePolicies = static_cast<OSDictionary *>(param3);
        thermalZones->setObject(acpiPath, service);
        activePolicies->setObject(acpiPath, zonePolicies);
    } else if (functionName == gDPTFUnregisterZone) {
        thermalZones->removeObject(acpiPath);
        activePolicies->removeObject(acpiPath);
    // Fans
    } else if (functionName == gDPTFRegisterFan) {
        fans->setObject(acpiPath, service);
    } else if (functionName == gDPTFUnregisterFan) {
        fans->removeObject(acpiPath);
    // Sensors
    } else if (functionName == gDPTFRegisterSensor) {
        sensors->setObject(acpiPath, service);
    } else if (functionName == gDPTFUnregisterSensor) {
        sensors->removeObject(acpiPath);
    } else {
        return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
    }
    
    return kIOReturnSuccess;
}

IOReturn ChultraThermal::newState() {
    //
    // Per zone:
    // 1. Get tripped active cooling levels
    // 2. Convert cooling levels to fan percentaages
    // 3. Get max fan level
    // 4. Set new fan level
    //
    
    IOLogDebug("Setting thermal states:");
    
    //
    // Iterate over Thermal Zones
    //
    OSCollectionIterator *zoneIter = OSCollectionIterator::withCollection(activePolicies);
    while (OSObject *zoneObj = zoneIter->getNextObject()) {
        
        OSSymbol *zoneKey = OSDynamicCast(OSSymbol, zoneObj);
        if (zoneKey == nullptr) continue;
        OSDictionary *zoneDict = OSDynamicCast(OSDictionary, activePolicies->getObject(zoneKey));
        if (zoneDict == nullptr) continue;
        
        IOLogInfo("\tZone %s:", zoneKey->getCStringNoCopy());
        
        //
        // Iterate over fans within this zone
        //
        OSCollectionIterator *fanIter = OSCollectionIterator::withCollection(zoneDict);
        while (OSObject *fanObj = fanIter->getNextObject()) {
            
            OSSymbol *fanKey = OSDynamicCast(OSSymbol, fanObj);
            if (fanKey == nullptr) continue;
            OSDictionary *fanDict = OSDynamicCast(OSDictionary, zoneDict->getObject(fanKey));
            if (fanDict == nullptr) continue;
            
            IOLogInfo("\t\tFan %s:", fanKey->getCStringNoCopy());
            
            uint32_t maxFanSpeed = 0;
            IOService *fanService = OSDynamicCast(IOService, fans->getObject(fanKey));
            if (fanService == nullptr) continue;
            
            //
            // Iterate over sensors and get their requested fan speeds
            //
            
            OSCollectionIterator *policyIter = OSCollectionIterator::withCollection(fanDict);
            while (OSObject *sensorPolicyObj = policyIter->getNextObject()) {
                
                OSSymbol *policyKey = OSDynamicCast(OSSymbol, sensorPolicyObj);
                if (policyKey == nullptr) continue;
                DPTFActivePolicyEntry *policy = OSDynamicCast(DPTFActivePolicyEntry, fanDict->getObject(policyKey));
                if (policy == nullptr) continue;
                
                //
                // Get active policy tripped level
                //
                
                OSObject *sensorObj = sensors->getObject(policy->source);
                IOService *sensorService = OSDynamicCast(IOService, sensorObj);
                if (sensorService == nullptr) continue;
                
                uint32_t trippedLevel;
                IOReturn ret = messageClient(kIOMessageDptfSensorReadLevel, sensorService, (void *) &trippedLevel);
                
                IOLogInfo("\t\t\tSensor %s: %d", policyKey->getCStringNoCopy(), trippedLevel);
                
                //
                // Turn tripped level into fan speed/command
                //
                
                uint32_t requestedSpeed = 0;
                if (trippedLevel < 10) {
                    requestedSpeed = policy->maxFanSpeeds[trippedLevel];
                    IOLogInfo("Requested Speed: %d", requestedSpeed);
                }
                
                maxFanSpeed = max(requestedSpeed, maxFanSpeed);
            }
            
            IOLogInfo("Fan set to %d", maxFanSpeed);
            IOReturn ret = messageClient(kIOMessageDptfFanSetLvl, fanService, (void *) &maxFanSpeed);
            OSSafeReleaseNULL(policyIter);
        }
        
        OSSafeReleaseNULL(fanIter);
    }
    
    OSSafeReleaseNULL(zoneIter);
    return kIOReturnSuccess;
}

IOReturn ChultraThermal::timerHandler(OSObject *, void *, void *, void *, void *) {
    newState();
    timer->setTimeoutMS(10000);
    return kIOReturnSuccess;
}
