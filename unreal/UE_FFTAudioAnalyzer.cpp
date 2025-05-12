#include "UE_FFTAudioAnalyzer.h"
#include "Kismet/GameplayStatics.h"
#include "AudioMixerBlueprintLibrary.h"

bool UE_FFTAudioAnalyzer::begin(void)
{
    bool success = false;

    UAudioMixerBlueprintLibrary::StopAnalyzingOutput(m_world);
    
    UAudioMixerBlueprintLibrary::StartAnalyzingOutput(
        m_world,
        nullptr,
        EFFTSize::Large,
        EFFTPeakInterpolationMethod::Linear,
        EFFTWindowType::Hann,
        0.0f,
        EAudioSpectrumType::MagnitudeSpectrum
    );
    
    success = FFTAudioAnalyzer::begin();

    return success;
}

void UE_FFTAudioAnalyzer::getMagnitudes(float* outMagnitudes) 
{
    TArray<float> UEMagnitudes;
    TArray<float> binFrequencies;
    
    binFrequencies.SetNum(NUM_TOTAL_BINS);
    
    for (int32 idx = 0; idx < NUM_LOW_BINS; idx++)
    {
        binFrequencies[idx] = LOW_FREQUENCIES[idx];
    }
    for (int32 idx = 0; idx < NUM_MID_BINS; idx++)
    {
        binFrequencies[idx + NUM_LOW_BINS] = MID_FREQUENCIES[idx];
    }
    for (int32 idx = 0; idx < NUM_HIGH_BINS; idx++)
    {
        binFrequencies[idx + NUM_LOW_BINS + NUM_MID_BINS] = HIGH_FREQUENCIES[idx];
    }

    UAudioMixerBlueprintLibrary::GetMagnitudeForFrequencies(
        m_world,
        binFrequencies,
        UEMagnitudes
    );
    
    memcpy(outMagnitudes, UEMagnitudes.GetData(), (NUM_TOTAL_BINS) * sizeof(float));
}

void UE_FFTAudioAnalyzer::getMaxMagnitudes(float* maxMagnitudes)
{
    memcpy(maxMagnitudes, UE::LOW_MAX_MAGNITUDES, NUM_LOW_BINS * sizeof(float));
    memcpy(maxMagnitudes + NUM_LOW_BINS, UE::MID_MAX_MAGNITUDES, NUM_MID_BINS * sizeof(float));
    memcpy(maxMagnitudes + NUM_LOW_BINS + NUM_MID_BINS, UE::HIGH_MAX_MAGNITUDES, NUM_HIGH_BINS * sizeof(float));
}
