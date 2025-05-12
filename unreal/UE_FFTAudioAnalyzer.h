#pragma once

#include "FFTAudioAnalyzer.h"
#include "Sound/SoundWave.h"
#include "AudioMixerBlueprintLibrary.h"
#include "NeonControllerActor.h"

class UE_FFTAudioAnalyzer : public FFTAudioAnalyzer {
public:
    UE_FFTAudioAnalyzer(PlatformUtils* utils) 
        : FFTAudioAnalyzer(utils)
        , m_world(nullptr)
    {}
    bool begin(void) override;
    void setWorld (UWorld* world) { m_world = world; }
protected:
    void getMagnitudes(float* outMagnitudes) override;
    void getMaxMagnitudes(float* maxMagnitudes) override;
private:
    UWorld* m_world;
};