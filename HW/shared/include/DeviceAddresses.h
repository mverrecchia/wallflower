// hardware/shared/include/DeviceAddresses.h
#pragma once
#include <stdint.h>
#include "PlatformConstants.h"

namespace DeviceAddresses {
    // Manager MAC
    constexpr uint8_t MANAGER_MAC[MAC_ADDRESS_SIZE] = {0x74, 0x4D, 0xBD, 0x9D, 0xAF, 0xCC};

    // Controller MACs
    // Number 9
    constexpr uint8_t CONTROLLER_0_MAC[MAC_ADDRESS_SIZE] = {0x24, 0xEC, 0x4A, 0x0E, 0xBB, 0x10};
    // White line
    constexpr uint8_t CONTROLLER_1_MAC[MAC_ADDRESS_SIZE] = {0xF0, 0x9E, 0x9E, 0x10, 0x98, 0x0C};
    // Number 5
    constexpr uint8_t CONTROLLER_2_MAC[MAC_ADDRESS_SIZE] = {0x24, 0xEC, 0x4A, 0x0E, 0xAE, 0xC8};
    // 24:ec:4a:0e:ae:c8
    // f0:9e:9e:10:98:fc
    // constexpr uint8_t CONTROLLER_3_MAC[MAC_ADDRESS_SIZE] = {0x24, 0x0A, 0xC4, 0x0D, 0x81, 0x46};
    // constexpr uint8_t CONTROLLER_4_MAC[MAC_ADDRESS_SIZE] = {0x24, 0x0A, 0xC4, 0x0D, 0x81, 0x44};
    // constexpr uint8_t CONTROLLER_5_MAC[MAC_ADDRESS_SIZE] = {0x24, 0x0A, 0xC4, 0x0D, 0x81, 0x45};
    // constexpr uint8_t CONTROLLER_6_MAC[MAC_ADDRESS_SIZE] = {0x24, 0x0A, 0xC4, 0x0D, 0x81, 0x46};
    // constexpr uint8_t CONTROLLER_7_MAC[MAC_ADDRESS_SIZE] = {0x24, 0x0A, 0xC4, 0x0D, 0x81, 0x44};
    // constexpr uint8_t CONTROLLER_8_MAC[MAC_ADDRESS_SIZE] = {0x24, 0x0A, 0xC4, 0x0D, 0x81, 0x45};
    // ... add more as needed

    // Helper array for iterating all controllers
    constexpr const uint8_t* CONTROLLER_MACS[] = {
        CONTROLLER_0_MAC,
        CONTROLLER_1_MAC,
        CONTROLLER_2_MAC,
        // CONTROLLER_3_MAC,
        // CONTROLLER_4_MAC,
        // CONTROLLER_5_MAC,
        // CONTROLLER_6_MAC,
        // CONTROLLER_7_MAC,
        // CONTROLLER_8_MAC,
    };

    constexpr size_t NUM_CONTROLLERS = sizeof(CONTROLLER_MACS) / sizeof(CONTROLLER_MACS[0]);
}