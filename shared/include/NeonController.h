#pragma once

#include "PlatformTypes.h"
#include "PlatformTimer.h"
#include "PlatformUtils.h"
#include "PlatformConstants.h"
#include "MessageTypes.h"
#include "ProfileExecutor.h"
#include <memory>
#include <cstring>

class NeonController {
public:
    NeonController(PlatformUtils* utils)
        : m_controllerState(ControllerState_E::IDLE)
        , m_targetSpeed(NORMALIZED_MIN)
        , m_distance(NORMALIZED_MIN)
        , m_managerPresent(false)
        , m_pUtils(utils)
        , m_profileExecutor(utils)
    {
        m_i2cWriteTimer.start();
        m_passiveBrightnessTimeoutTimer.start();
        for (size_t idx = 0; idx < NUM_SUPPLIES_PER_CONTROLLER; idx++)
        {
            m_supplies[idx].id = idx;
            m_supplies[idx].brightness = 0.0f;
            m_supplies[idx].enable = false;
            m_supplies[idx].current = 0.0f;
        }
        m_motor.speed = 0.0f;
        m_motor.acceleration = 0.0f;
        m_motor.direction = true;
        m_motor.enable = false;
    }
    
    virtual ~NeonController() = default;

    // Main update
    virtual void update(float deltaTime);

    // Initialization
    void setMacAddress(const uint8_t* macAddress) { memcpy(m_macAddress, macAddress, MAC_ADDRESS_SIZE); }
    // Motion control interface - speeds/acceleration are set between 0.00 - 1.00
    virtual void setSpeed(float speed)
    {
        m_targetSpeed = m_pUtils->clamp(std::abs(speed), NORMALIZED_MIN, NORMALIZED_MAX);
    }

    virtual void setAcceleration(float acceleration) { m_motor.acceleration = acceleration; }
    virtual void setDirection(bool direction)        { m_motor.direction = direction; }
    virtual void setMotorEnable(bool enable)         { m_motor.enable = enable; }

    virtual void setSupplyBrightness(size_t idx, float brightness)   { m_supplies[idx].brightness = brightness; };
    virtual void setSupplyEnable(size_t idx, bool enable)            { m_supplies[idx].enable = enable; };

    // Getters for motor state - speeds/acceleration return between 0.00 - 1.00
    virtual float getTargetSpeed(void) const  { return m_targetSpeed; }

    virtual bool getDirection(void) const              { return m_motor.direction; }
    virtual bool getEnable(void) const                 { return m_motor.enable; }

    virtual bool getSupplyEnable(size_t idx) const     { return m_supplies[idx].enable; }
    float getSupplyBrightness(size_t idx) const   { return m_supplies[idx].brightness; }

    float getSupplyCurrent(size_t idx) const     { return m_supplies[idx].current; }
    float getDistance(void) const                { return m_distance; }
    bool getDistanceUnderThreshold(void) const   { return m_distance < DISTANCE_OVERRIDE_THRESHOLD; }
    OperatingMode_E getOperatingMode(void) const { return m_operatingMode; }

    bool getManagerPresent(void) const           { return m_managerPresent; }
    ControllerState_E getControllerState() const { return m_controllerState; }

    virtual void setDistance(float distance)         { m_distance = distance; }
    void setSupplyCurrent(size_t idx, float current) { m_supplies[idx].current = current; }
    void setManagerPresent(bool present)             { m_managerPresent = present; }
    void setControllerState(ControllerState_E state) { m_controllerState = state; }
    void setOperatingMode(OperatingMode_E mode)      { m_operatingMode = mode; }

    // Platform specific
    virtual void runMotorSpeed() = 0;
    virtual void runMotor() = 0;
    virtual void stopMotor() = 0;

    // Add these methods to handle profile control
    void startProfileExecutor(const ProfileRequestParameters_S& profile)
    {
        m_profileExecutor.startProfile(profile);
    }

    void stopProfileExecutor(void)
    {
        m_profileExecutor.stopProfile();
    }

    void updateProfileExecutor(float deltaTime, bool brightnessEnable, bool motorEnable);

    void updateAudioActivity(float deltaTime);

    // Used by UE and HW
    void handleHeartbeatMessage(const ManagerHeartbeatMessageFull_S* msg);
    void handleProfileMessage(const ProfileMessageFull_S* msg);
    void handleAudioMessage(const AudioMessageFull_S* msg);
    void handleManualMessage(const ManualMessageFull_S* msg);

    void queueHeartbeatMessage(void);

protected:
    // Common functionality
    void updateConnectionState(float deltaTime);
    void updateModes(float deltaTime);
    void runDistanceOverride(float deltaTime);
    void updateDistanceOverrideValues(float distance, float& rotatingValue, float& stationaryValue, float& motorValue, bool& direction);

    void enterProfileMode(void);
    void enterAudioMode(void);
    void enterPassiveMode(void);
    void enterIdleMode(void);

    void runPassiveMode(float deltaTime);
    void runAudioMode(float deltaTime);
    void runProfileMode(float deltaTime);
    void runManualMode(float deltaTime);
    void runIdleMode(float deltaTime);

    virtual void updateDistanceValue(void) = 0;
    virtual void updateOperatingMode(void) = 0;

    virtual void applyMotorSettings(void) = 0;
    virtual void applyMotorSpeed(void) = 0;

    virtual void applyNeonSettings(void) = 0;

    virtual void queueMessage(MessageType_E messageType, const void* msg) = 0;

    uint8_t m_macAddress[MAC_ADDRESS_SIZE];

    // States
    ControllerState_E m_controllerState;

    // Instantaneous states
    SupplyParameters_S m_supplies[NUM_SUPPLIES_PER_CONTROLLER];
    MotorParameters_S m_motor;

    // Manual request states
    SupplyParameters_S m_manualRequestSupplies[NUM_SUPPLIES_PER_CONTROLLER];
    MotorParameters_S m_manualRequestMotor;

    // Audio request states
    SupplyParameters_S m_audioRequestSupplies[NUM_SUPPLIES_PER_CONTROLLER];
    MotorParameters_S m_audioRequestMotor;

    // Distance override states (replaces "last")
    SupplyParameters_S m_distanceOverrideSupplies[NUM_SUPPLIES_PER_CONTROLLER];
    MotorParameters_S m_distanceOverrideMotor;

    // Profile states (replaces "initial")
    SupplyParameters_S m_profileSupplies[NUM_SUPPLIES_PER_CONTROLLER];
    MotorParameters_S m_profileMotor;

    // Controller measurements
    float m_distance;
    OperatingMode_E m_operatingMode;
    bool m_isInDistanceOverride = false;
    float m_transitionTimer = 0.0f;

    // These are all normalized to 0-1 and scaled accordingly in UE/HW
    float m_targetSpeed;

    bool m_brightnessChangeEnable;
    bool m_motorChangeEnable;

    bool m_managerPresent;

    PlatformUtils* m_pUtils;
    ProfileExecutor m_profileExecutor;
    // State transition timeouts
    Timer m_connectionTimer;
    Timer m_i2cWriteTimer;
    Timer m_passiveBrightnessTimeoutTimer;
    Timer m_distanceOverrideTimer;
    // Timer m_transitionTimer;
    Timer m_audioActivityTimer;
    Timer m_manualModeTimer;

    bool m_wasInProfileMode;
    bool m_wasInAudioMode;
    bool m_wasInDistanceOverride;

    bool m_audioActive = false;
    bool m_profileActive = false;
    bool m_manualModeActive = false;
    bool m_idleModeActive = false;
};