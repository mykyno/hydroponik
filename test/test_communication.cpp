/**
 * @file test_communication.cpp  
 * @brief Basic communication manager test
 */

#include <Arduino.h>
#include <unity.h>
#include "communication.h"

// Test WiFi credentials (dummy for testing)
const char* TEST_SSID = "TestNetwork";
const char* TEST_PASSWORD = "TestPassword123";

void test_communication_init() {
    // Test basic initialization
    communication_init(TEST_SSID, TEST_PASSWORD);
    
    // Verify global Debug instance created
    TEST_ASSERT_NOT_NULL(Debug);
    
    // Verify initial state is SERIAL_ONLY (WiFi will fail to connect)
    delay(1000);  // Allow state to stabilize
    
    // Should start in SERIAL_ONLY since no actual WiFi available
    CommState state = Debug->get_state();
    TEST_ASSERT(state == CommState::SERIAL_ONLY || state == CommState::WIFI_CONNECTING);
}

void test_debug_output() {
    // Test unified output interface
    if (Debug) {
        Debug->println("Test message from communication manager");
        Debug->printf("Test formatted message: %d", 42);
        
        // Should not crash
        TEST_ASSERT_TRUE(true);
    }
}

void test_status_reporting() {
    if (Debug) {
        String status = communication_get_status();
        TEST_ASSERT_TRUE(status.length() > 0);
        TEST_ASSERT_TRUE(status.indexOf("Communication Status") >= 0);
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    UNITY_BEGIN();
    
    RUN_TEST(test_communication_init);
    RUN_TEST(test_debug_output);
    RUN_TEST(test_status_reporting);
    
    UNITY_END();
}

void loop() {
    // Test complete
}