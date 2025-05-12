#pragma once

#include "NeonManager.h"
#include "UE_PlatformUtils.h"
#include "UE_PlatformTypes.h"
#include "UE_FFTAudioAnalyzer.h"
#include "UE_NeonController.h"
#include "CoreMinimal.h"
#include "Sound/SoundWave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "Dom/JsonObject.h"

class UE_NeonManager : public NeonManager {
public:
    UE_NeonManager()
        : NeonManager(new UE_PlatformUtils())
        , m_world(nullptr)
    {
        m_pFftAnalyzer = new UE_FFTAudioAnalyzer(m_pUtils);
    }
    void setWorld(UWorld* world) { m_world = world; }
    void update(float deltaTime);
    virtual bool initialize(void) override;

    UE_NeonController* m_controllers[NUM_CONTROLLERS];

protected:
    virtual void queueMessage(MessageType_E messageType, size_t idx, const void* msg) override;

private:
    UWorld* m_world;

    void spoofHeartbeatMessages(size_t idx, const void* msg);
    void spoofManualMessages(size_t idx, const void* msg);
    void spoofAudioMessages(size_t idx, const void* msg);
    void spoofProfileMessages(size_t idx, const void* msg);
    void spoofBezierMessages(size_t idx, const void* msg);

    void appendMacAddress(uint8_t* mac, size_t idx) override;
};