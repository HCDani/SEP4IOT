#include <unity.h>
#include <avrtos\avrtos.h>
#include "wifi.h"
#include "includes.h"

#ifndef PIO_UNIT_TESTING
#endif

//Clean the project before testing. Sometimes it doesn't recognizes the test cases.

void setUp(void) {
  // set stuff up here
}

void tearDown(void) {
  // clean stuff up here
}

void test_wifi_read(void){
  WIFI_ERROR_MESSAGE_t result;
// Initialize WiFi
  wifi_init();
// Test sending an AT command and receiving a response
  result = wifi_command_AT();
  TEST_ASSERT_EQUAL(WIFI_OK, result);
}

int runUnityTests(void) {
  sei();
  z_avrtos_init();
  UNITY_BEGIN();
  RUN_TEST(test_wifi_read);
 
return UNITY_END();
}

int main(void) {
  return runUnityTests();
}
void setup() {
  // Wait ~2 seconds before the Unity test runner
  // establishes connection with a board Serial interface
  k_sleep(K_SECONDS(2));

  runUnityTests();
}
void loop() {}
