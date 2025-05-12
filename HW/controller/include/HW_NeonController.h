// HW_NeonController.h
#pragma once

#include "NeonController.h"
#include "DistanceSensor.h"
#include "DeviceAddresses.h"
#include "HW_PlatformUtils.h"
#include "PlatformUtils.h"
#include "PlatformTypes.h"
#include "PlatformConstants.h"
#include "MessageTypes.h"
#include <esp_now.h>
#include <esp_wifi.h>
#include <Arduino.h>
#include <AccelStepper.h>
#include <Adafruit_MCP4728.h>

class HW_NeonController : public NeonController {
public:
    HW_NeonController() 
        : NeonController(new HW_PlatformUtils())
        , m_stepper(AccelStepper::DRIVER, PIN_MOTOR_STEP, PIN_MOTOR_DIR)
    {}

    bool initialize(void);
    void update(float deltaTime) override;

    // Supply control
    void applySupplyEnable(size_t idx);
    void applySupplyBrightness(size_t idx);

    // Motor control 
    void applyMotorEnable(void);
    void applyMotorDirection(void);
    void applyMotorSpeed(void) override;
    void applyMotorMaxSpeed(void);
    void applyMotorAcceleration(void);
    void runMotorSpeed(void) override;
    void runMotor(void) override;
    void stopMotor(void) override;
    
    void setEnable(bool enable) { setMotorEnable(enable); }
    void setTargetSpeed(float speed) { setSpeed(speed); }

    uint8_t* getManagerMac() { return m_managerMac; }
    void setManagerMac(const uint8_t* mac)
    {
        memcpy(m_managerMac, mac, 6);
        m_managerRegistered = true;
    }
    bool getManagerRegistered(void) const { return m_managerRegistered; }

    DistanceSensor& getDistanceSensor() { return m_distanceSensor; }
    PlatformUtils* getUtils() { return m_pUtils; }

    void queueMessage(MessageType_E messageType, const void* msg) override;
    void sendMessage(MessageType_E messageType, const void* msg, uint8_t messageSize);
    void handleEspNowMessage(const uint8_t* data, int len);

    bool isProfileActive() const { return m_profileActive; }
    void stopProfile() { m_profileActive = false; }
    
    void applyMotorSettings(void) override;
    void applyNeonSettings(void) override;

protected:
    void updateOperatingMode(void) override;
    void updateDistanceValue(void) override;

private:    
    uint8_t m_managerMac[MAC_ADDRESS_SIZE];
    bool m_managerRegistered = false;
    bool m_profileActive = false;

    DistanceSensor m_distanceSensor;
    AccelStepper m_stepper;
    Adafruit_MCP4728 m_dac;
    SupplyOutputType_E m_supplyOutputType[NUM_SUPPLIES_PER_CONTROLLER];

    void initializeI2C(void);
    void initializeMotor(void);
    void initializeSupplies(void);

    void applyDACOutput(MCP4728_channel_t dacChannel, float voltage);
};