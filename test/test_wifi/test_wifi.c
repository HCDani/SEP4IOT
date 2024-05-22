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
  //I need to add this method: uint32_t wifi_ntpTime();
  //I need to add this method:http_get
}
//add
void test_wifi_ntp_time(void) {
  // Initialize WiFi
  wifi_init();

  // Get the current time from NTP
  uint32_t ntp_time = wifi_ntpTime();
  printf("NTP Time: %u\n", ntp_time);  // Debugging output
  TEST_ASSERT_NOT_EQUAL(0, ntp_time);  // Assuming 0 is an invalid time
}

void test_wifi_http_get(void) {
  // Initialize WiFi
  wifi_init();

  // Perform an HTTP GET request
  char response[256];
  const char *request = "GET / HTTP/1.1\r\nHost: example.com\r\n\r\n";
  WIFI_ERROR_MESSAGE_t result = wifi_http_get("example.com", 80, request, response, sizeof(response));
  TEST_ASSERT_EQUAL(WIFI_OK, result);
  TEST_ASSERT_NOT_NULL(strstr(response, "200 OK"));  // Assuming the response contains "200 OK"
}

int runUnityTests(void) {
  sei();
  z_avrtos_init();
  UNITY_BEGIN();
  RUN_TEST(test_wifi_read);
  RUN_TEST(test_wifi_ntp_time);
  RUN_TEST(test_wifi_http_get);
 
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
