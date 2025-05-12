// shared/include/NeonManager.h

#include "FFTAudioAnalyzer.h"
#include "AudioOrchestrator.h"
#include "PlatformUtils.h"
#include "PlatformTypes.h"
#include "PlatformTimer.h"
#include "MessageTypes.h"

#include <memory>
#include <cstddef>

class NeonManager {
public:
    NeonManager(PlatformUtils* utils, FFTAudioAnalyzer* fftAnalyzer = nullptr)
        : m_pUtils(utils)
        , m_pFftAnalyzer(fftAnalyzer)
        , m_pAudioOrchestrator(new AudioOrchestrator(m_pUtils))
        , m_managerState(ManagerState_E::IDLE)
        , m_expectedControllerCount(NUM_CONTROLLERS)
        , m_connectedControllerCount(NUM_CONTROLLERS)
    {
        m_audioTimer.start();
    }
    virtual ~NeonManager() = default;

    virtual bool initialize(void) = 0;
    virtual void update(float deltaTime);
    void updateManagerState(float deltaTime);
    void updateInputFlags(float deltaTime);
    void updateControllerTimeouts(float deltaTime);
    void updateAudioListening(float deltaTime);

    FFTAudioAnalyzer* getFFTAnalyzer() const        { return m_pFftAnalyzer; }
    ManagerState_E getManagerState(void) const      { return m_managerState; }

    size_t getConnectedControllerCount(void) const  { return m_connectedControllerCount; }
    size_t getExpectedControllerCount(void) const   { return m_expectedControllerCount;  }

    bool getControllerPaired() const  { return (m_managerState == ManagerState_E::ACTIVE_FULL || 
                                                m_managerState == ManagerState_E::ACTIVE_PARTIAL); }

    // Public for UE implementation
    void setConnectedControllerCount(size_t count)           { m_connectedControllerCount = count; }
    void setExpectedControllerCount(size_t count)            { m_expectedControllerCount  = count; }
    void setControllerProfileActive(size_t idx, bool active) { m_controllerStateParameters[idx].profileActive = active; }
    void setControllerActive(size_t idx, bool active)
    {
        m_controllerActive[idx] = active;
        m_controllerTimeouts[idx].start();
    }

    void sendProfilesToControllers()
    {
        for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
        {
            queueProfileMessage(idx);
        }
    }

    // Apply profiles to multiple controllers with individual configurations
    void applyProfilesToControllers(const ProfileRequestParameters_S profiles[NUM_CONTROLLERS])
    {
        for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
        {
            // Update the profile parameters for each controller
            m_profileRequestParameters[idx] = profiles[idx];
        }
        
        // Trigger profile mode to send all profiles
        m_directControlProfile = true;
    }

    // Apply the same profile to all controllers
    void applyProfileToAllControllers(const ProfileRequestParameters_S& profile)
    {
        for (size_t idx = 0; idx < NUM_CONTROLLERS; idx++)
        {
            m_profileRequestParameters[idx] = profile;
        }
        
        // Trigger profile mode to send all profiles
        m_directControlProfile = true;
    }

    // Helper functions to set individual profile parameters
    void setControllerProfileType(size_t idx, ProfileType_E type)
    {
        if (idx < NUM_CONTROLLERS)
        {
            m_profileRequestParameters[idx].type = type;
        }
    }

    void setControllerProfileMagnitude(size_t idx, float magnitude)
    {
        if (idx < NUM_CONTROLLERS)
        {
            m_profileRequestParameters[idx].magnitude = magnitude;
        }
    }

    void setControllerProfileFrequency(size_t idx, float frequency)
    {
        if (idx < NUM_CONTROLLERS)
        {
            m_profileRequestParameters[idx].frequency = frequency;
        }
    }

    void setControllerProfilePhase(size_t idx, float phase)
    {
        if (idx < NUM_CONTROLLERS)
        {
            m_profileRequestParameters[idx].phase = phase;
        }
    }

    void setControllerProfileEnable(size_t idx, bool enable)
    {
        if (idx < NUM_CONTROLLERS)
        {
            m_profileRequestParameters[idx].enable = enable;
        }
    }

    void setControllerProfileStop(size_t idx, bool stopProfile)
    {
        if (idx < NUM_CONTROLLERS)
        {
            m_profileRequestParameters[idx].stopProfile = stopProfile;
        }
    }

    // Get current profile parameters for a controller
    const ProfileRequestParameters_S& getControllerProfile(size_t idx) const
    {
        static ProfileRequestParameters_S defaultProfile = {};
        if (idx < NUM_CONTROLLERS)
        {
            return m_profileRequestParameters[idx];
        }
        return defaultProfile;
    }

    void setAudioConfig(AudioConfiguration_S audioConfig)
    {
        m_pAudioOrchestrator->setAudioConfig(audioConfig);
    }

    void queueHeartbeatMessage(void);

protected:
    void setManagerState(ManagerState_E managerState) { m_managerState = managerState; }

    void runManualMode(float deltaTime);
    void runProfileMode(float deltaTime);
    void runAudioMode(float deltaTime);
    void runDefaultMode(float deltaTime);

    void queueManualMessage(size_t idx);
    void queueProfileMessage(size_t idx);
    void queueAudioMessage(void);

    virtual void appendMacAddress(uint8_t* mac, size_t idx) = 0;
    // This function may either directly send the message or queue it for later sending
    // depending on the platform implementation
    virtual void queueMessage(MessageType_E messageType, size_t idx, const void* msg) = 0;
    
    void manageDirectControlFlags(void);

    // Common state
    PlatformUtils* m_pUtils;
    FFTAudioAnalyzer* m_pFftAnalyzer;
    AudioOrchestrator* m_pAudioOrchestrator;

    // State management
    ManagerState_E m_managerState;

    size_t m_expectedControllerCount;
    size_t m_connectedControllerCount;

    ControllerStateParameters_S m_controllerStateParameters[NUM_CONTROLLERS];
    ControllerRequestParameters_S m_controllerRequestParameters[NUM_CONTROLLERS];
    ProfileRequestParameters_S m_profileRequestParameters[NUM_CONTROLLERS];

    // Controller states
    bool m_controllerActive[NUM_CONTROLLERS] = {false};
    Timer m_controllerTimeouts[NUM_CONTROLLERS];

    // Main state inputs
    bool m_audioReactivityEnabled = true;
    bool m_directControlAudio = false;
    bool m_directControlProfile = false;
    bool m_directControlManual = false;

private:
    Timer m_stateTimer;
    Timer m_audioTimer;
    BroadcastType_E m_broadcastType;

    bool m_pulseReady = false;
};