#pragma once
#include <Arduino.h>
#include "SwipeAddState.h"
#include "PinAuthState.h"
#include "DoorLockState.h"
#include "RuntimeFlags.h"
#include "DeviceState.h"

struct AppState
{
    // Device info
    String deviceMAC = "";
    String baseTopic = "";
    String defaultTopic = "door/";
    
    // Component states
    SwipeAddState swipeAdd;         // Swipe-to-add card state
    PinAuthState pinAuth;           // PIN authentication state
    DoorLockState doorLock;         // Door lock state
    RuntimeFlags flags;             // Runtime flags
    DeviceState device;             // Device connection state
    
    // Periodic task timers
    uint32_t lastBatteryPublishMs = 0;
    uint32_t lastSyncMs = 0;
    
    static constexpr uint32_t BATTERY_PUBLISH_INTERVAL_MS = 60000;  // 1 min
    static constexpr uint32_t SYNC_INTERVAL_MS = 300000;            // 5 min
    
    void init(const String& mac)
    {
        deviceMAC = mac;
        
        // Generate default topic from MAC
        String mac2 = mac;
        mac2.replace(":", "");
        mac2.toLowerCase();
        defaultTopic = "door/" + mac2.substring(0, 6);
        
        baseTopic = defaultTopic;
        
        // Initialize component states
        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        flags.reset();
        
        lastBatteryPublishMs = 0;
        lastSyncMs = 0;
    }
    
    void setBaseTopic(const String& topic)
    {
        if (topic.length() > 0) {
            baseTopic = topic;
        } else {
            baseTopic = defaultTopic;
        }
    }
    
    bool shouldPublishBattery() const
    {
        return (millis() - lastBatteryPublishMs) >= BATTERY_PUBLISH_INTERVAL_MS;
    }
    
    void markBatteryPublished()
    {
        lastBatteryPublishMs = millis();
    }
    
    bool shouldSync() const
    {
        return (millis() - lastSyncMs) >= SYNC_INTERVAL_MS;
    }
    
    void markSynced()
    {
        lastSyncMs = millis();
    }
    
    void reset()
    {
        swipeAdd.reset();
        pinAuth.reset();
        doorLock.lock();
        flags.reset();
        lastBatteryPublishMs = 0;
        lastSyncMs = 0;
    }
};