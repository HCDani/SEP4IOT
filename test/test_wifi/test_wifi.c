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
  
 char response[128];
  WIFI_ERROR_MESSAGE_t result;

  // Initialize WiFi
  wifi_init();

  // Test sending an AT command and receiving a response
  result = wifi_command_with_response("AT", response, sizeof(response), 10);
  TEST_ASSERT_EQUAL(WIFI_OK, result);
  TEST_ASSERT_NOT_NULL(strstr(response, "OK"));   

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
